// ParadoxMake: builds and manages Paradox Engine projects, the way qmake does for Qt.
//
//   paradoxmake [command] [options] [project-folder]
//
// A project folder holds a paradoxmakefile.yml. ParadoxMake reads it, finds the projects it depends on (sibling folders with a
// paradoxmakefile.yml), and generates a build tree. CMake does the compiling (it already knows Qt, SDL2, install rules and ctest); the
// yml is what a Paradox project is described with.
//
// Commands: generate (write the build files), configure (generate + run cmake), build (the default: configure + compile),
//           install, test, clean, info (print the resolved project graph).
// Options:  -B <dir> build folder (default <root>/build), -D<NAME>=<VALUE> passed to cmake (-DCMAKE_BUILD_TYPE=Release, -DSPLASH=ON...),
//           --prefix <dir> install prefix, -j <n> jobs.
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

#ifndef PARADOXMAKE_DATA_DIR
#define PARADOXMAKE_DATA_DIR "."
#endif

// Where options.cmake and prelude.cmake are: $PARADOXMAKE_DATA, else installed beside the tool (<prefix>/share/paradoxmake/cmake), else the source
// tree this copy was built from.
static fs::path dataDir()
{
	if (const char *e = std::getenv("PARADOXMAKE_DATA")) return e;
	std::error_code ec;
	fs::path self = fs::read_symlink("/proc/self/exe", ec); // Linux only, like the rest of the build for now
	if (!ec) {
		fs::path installed = self.parent_path().parent_path() / "share" / "paradoxmake" / "cmake";
		if (fs::exists(installed / "prelude.cmake")) return installed;
	}
	return PARADOXMAKE_DATA_DIR;
}

struct Project {
	fs::path dir;
	std::string name, version, license, type, main, executable, coreLibrary, cmakeFragment;
	std::string resourceDir, resourceOut, asmDir, asmOut;
	struct Reuse { std::string project; std::vector<std::string> except; bool main = false; };
	std::vector<Reuse> reuse;   // projects whose code and resources this one starts from (the in-between games: Sonic 2 begins as Sonic 1)
	bool reuseOnly = false;     // loaded only to be reused from: not built as a target of its own
	std::string songDir;
	std::vector<std::string> depends, include, sources, resources, assemble;
	std::vector<std::pair<std::string, std::string>> songs; // {jsonc file, C array name}
};

static std::vector<std::string> strList(const YAML::Node &n)
{
	std::vector<std::string> v;
	if (n && n.IsSequence()) for (const auto &i : n) v.push_back(i.as<std::string>());
	else if (n && n.IsScalar()) v.push_back(n.as<std::string>());
	return v;
}

static std::string str(const YAML::Node &n, const char *key, const char *def = "")
{
	return n[key] ? n[key].as<std::string>() : def;
}

static bool loadProject(const fs::path &dir, Project &p)
{
	fs::path file = dir / "paradoxmakefile.yml";
	if (!fs::exists(file)) return false;
	YAML::Node y = YAML::LoadFile(file.string());
	p.dir = fs::weakly_canonical(dir);
	p.name = str(y, "project");
	p.version = str(y, "version", "1.0");
	p.license = str(y, "license");
	p.type = str(y, "type", "game");
	p.main = str(y, "main");
	p.executable = str(y, "executable");
	p.coreLibrary = str(y, "core_library");
	p.cmakeFragment = str(y, "cmake");
	p.resourceDir = str(y, "resource_dir", "res");
	p.resourceOut = str(y, "resource_out", "src/Resource");
	p.asmDir = str(y, "asm_dir", "asm");
	p.asmOut = str(y, "asm_out", "res");
	p.depends = strList(y["depends"]);
	p.include = strList(y["include"]);
	p.sources = strList(y["sources"]);
	p.resources = strList(y["resources"]);
	p.assemble = strList(y["assemble"]);
	p.songDir = str(y, "song_dir", "res/SMPS/converted");
	if (y["reuse"]) for (const auto &n : y["reuse"]) {
		Project::Reuse r;
		r.project = n["project"].as<std::string>();
		r.except = strList(n["except"]);
		r.main = n["main"] && n["main"].as<bool>();
		p.reuse.push_back(r);
	}
	if (y["songs"]) for (const auto &n : y["songs"]) p.songs.push_back({n["file"].as<std::string>(), n["name"].as<std::string>()});
	if (p.name.empty()) { std::cerr << file << ": missing 'project:'\n"; return false; }
	return true;
}

// The project's dependencies live next to it (or next to the folder it is in): scan the siblings for a matching 'project:'.
static fs::path findProject(const fs::path &from, const std::string &name)
{
	for (const fs::path &base : {from.parent_path(), from.parent_path().parent_path()}) {
		if (base.empty() || !fs::is_directory(base)) continue;
		for (const auto &e : fs::directory_iterator(base)) {
			Project q;
			if (e.is_directory() && loadProject(e.path(), q) && q.name == name) return e.path();
		}
	}
	return {};
}

static bool resolve(const fs::path &dir, std::map<std::string, Project> &all, std::vector<std::string> &order, std::set<std::string> &stack)
{
	Project p;
	if (!loadProject(dir, p)) { std::cerr << "paradoxmake: no paradoxmakefile.yml in " << dir << "\n"; return false; }
	if (all.count(p.name)) return true;
	if (!stack.insert(p.name).second) { std::cerr << "paradoxmake: dependency cycle at " << p.name << "\n"; return false; }
	for (const auto &d : p.depends) {
		fs::path dd = findProject(p.dir, d);
		if (dd.empty()) { std::cerr << "paradoxmake: " << p.name << " depends on '" << d << "', which was not found beside it\n"; return false; }
		if (!resolve(dd, all, order, stack)) return false;
	}
	for (const auto &r : p.reuse) {
		if (all.count(r.project)) continue;
		fs::path dd = findProject(p.dir, r.project);
		if (dd.empty()) { std::cerr << "paradoxmake: " << p.name << " reuses '" << r.project << "', which was not found beside it\n"; return false; }
		std::vector<std::string> unused;
		std::set<std::string> st2;
		if (!resolve(dd, all, unused, st2)) return false;
		// what the reused project depends on is built (the engine) only if this project depends on it too; the project itself is only read from
		if (std::find(order.begin(), order.end(), r.project) == order.end()) all[r.project].reuseOnly = true;
	}
	stack.erase(p.name);
	order.push_back(p.name);
	all[p.name] = p;
	return true;
}

static std::string q(const std::string &s) { return "\"" + s + "\""; }
static std::string qp(const fs::path &p) { return q(p.generic_string()); }

static void emitList(std::ostream &o, const fs::path &base, const std::vector<std::string> &v)
{
	for (const auto &s : v) o << "  " << qp(base / s) << "\n";
}

static std::string cmakeName(const Project &p) { return p.type == "shared-library" ? p.name : (p.coreLibrary.empty() ? p.name : p.coreLibrary); }

static void emitProject(std::ostream &o, const Project &p, const std::map<std::string, Project> &all)
{
	std::string tgt = cmakeName(p);
	o << "\n# ---- " << p.name << " (" << p.type << ", " << (p.license.empty() ? "no licence stated" : p.license) << ") ----\n";
	o << "set(PM_DIR " << qp(p.dir) << ")\n";
	if (p.type == "shared-library") o << "add_library(" << tgt << " SHARED\n";
	else o << "add_library(" << tgt << " STATIC\n";
	emitList(o, p.dir, p.sources);
	for (const auto &r : p.reuse) { // the reused project's sources, but the ones this project replaces
		const Project &rp = all.at(r.project);
		for (const auto &s : rp.sources)
			if (std::find(r.except.begin(), r.except.end(), s) == r.except.end()) o << "  " << qp(rp.dir / s) << "\n";
	}
	o << ")\n";
	if (p.type == "game") o << "set_target_properties(" << tgt << " PROPERTIES POSITION_INDEPENDENT_CODE ON)\n";
	for (const auto &i : p.include) o << "target_include_directories(" << tgt << " PUBLIC " << qp(p.dir / i) << ")\n";
	for (const auto &r : p.reuse)
		for (const auto &i : all.at(r.project).include) o << "target_include_directories(" << tgt << " PUBLIC " << qp(all.at(r.project).dir / i) << ")\n";
	for (const auto &d : p.depends) {
		const Project &dp = all.at(d);
		o << "target_link_libraries(" << tgt << " PUBLIC " << cmakeName(dp) << ")\n";
	}
	if (p.type == "game") {
		std::string mainFile = p.main.empty() ? "" : (p.dir / p.main).generic_string();
		for (const auto &r : p.reuse)
			if (r.main && mainFile.empty()) mainFile = (all.at(r.project).dir / all.at(r.project).main).generic_string();
		if (p.executable.empty() || mainFile.empty()) o << "# (no executable/main: a library-only game)\n";
		else {
			// The engine calls back into the game for what only a game can say (GameInterface.h): those symbols stay undefined in the shared
			// engine and are resolved by the executable, so the game's static library is linked whole. Linux/ELF only as it stands.
			o << "function(link_game TARGET)\n  target_link_libraries(${TARGET} PRIVATE";
			for (const auto &d : p.depends) o << " " << cmakeName(all.at(d));
			o << " \"-Wl,--whole-archive\" " << tgt << " \"-Wl,--no-whole-archive\")\nendfunction()\n";
			o << "add_executable(" << p.executable << " WIN32 " << q(mainFile) << ")\n";
			o << "link_game(" << p.executable << ")\n";
		}
	}
	o << "set(PM_RESOURCES";
	for (const auto &r : p.resources) o << "\n  " << q(r);
	o << ")\n";
	// what the reused projects are built from lands in this project's own Resource folder (assembled files in the reused project's res, as it does itself)
	for (const auto &r : p.reuse) {
		const Project &rp = all.at(r.project);
		for (const auto &sg : rp.songs)
			o << "pm_add_song(" << tgt << " " << qp(rp.dir / rp.songDir) << " " << qp(p.dir / p.resourceOut / "Music") << " " << q(sg.first) << " " << sg.second << ")\n";
		// (an except: entry "<asm dir>/<name>.asm" drops that assembled file, "<resource dir>/<name>" that resource: the game has its own)
		auto dropped = [&](const std::string &entry) { return std::find(r.except.begin(), r.except.end(), entry) != r.except.end(); };
		std::vector<std::string> asmKept, resKept;
		for (const auto &a : rp.assemble)
			if (!dropped((fs::path(rp.asmDir) / (a + ".asm")).generic_string())) asmKept.push_back(a);
		for (const auto &a : rp.resources)
			if (!dropped((fs::path(rp.resourceDir) / a).generic_string())) resKept.push_back(a);
		if (!asmKept.empty()) {
			o << "pm_assemble(" << tgt << " " << qp(rp.dir / rp.asmDir) << " " << qp(rp.dir / rp.asmOut);
			for (const auto &a : asmKept) o << "\n  " << q(a);
			o << ")\n";
		}
		if (!resKept.empty()) {
			o << "pm_convert_resources(" << tgt << " " << qp(rp.dir / rp.resourceDir) << " " << qp(p.dir / p.resourceOut);
			for (const auto &a : resKept) o << "\n  " << q(a);
			o << ")\n";
		}
	}
	if (!p.cmakeFragment.empty()) o << "include(" << qp(p.dir / p.cmakeFragment) << ")\n";
	for (const auto &sg : p.songs)
		o << "pm_add_song(" << tgt << " " << qp(p.dir / p.songDir) << " " << qp(p.dir / p.resourceOut / "Music") << " " << q(sg.first) << " " << sg.second << ")\n";
	o << "pm_convert_resources(" << tgt << " " << qp(p.dir / p.resourceDir) << " " << qp(p.dir / p.resourceOut) << " ${PM_RESOURCES})\n";
	if (!p.assemble.empty()) {
		o << "pm_assemble(" << tgt << " " << qp(p.dir / p.asmDir) << " " << qp(p.dir / p.asmOut);
		for (const auto &a : p.assemble) o << "\n  " << q(a);
		o << ")\n";
	}
}

static std::string generate(const fs::path &root, const std::map<std::string, Project> &all, const std::vector<std::string> &order)
{
	const Project &top = all.at(order.back());
	std::ostringstream o;
	o << "# Generated by ParadoxMake from the paradoxmakefile.yml files. Do not edit.\n";
	o << "cmake_minimum_required(VERSION 3.16)\n";
	o << "set(PM_ROOT " << qp(root) << ")\n";
	o << "set(PM_DATA " << qp(dataDir()) << ")\n";
	o << "include(\"${PM_DATA}/options.cmake\")\n";
	o << "project(" << top.name << " VERSION " << top.version << " LANGUAGES C CXX)\n";
	o << "include(\"${PM_DATA}/prelude.cmake\")\n";
	for (const auto &n : order) emitProject(o, all.at(n), all);
	return o.str();
}

static int run(const std::string &cmd)
{
	std::cerr << "+ " << cmd << "\n";
	int r = std::system(cmd.c_str());
	return r == 0 ? 0 : 1; // system() gives a wait status: 256 would be 0 as an exit code
}

static std::string sh(const std::string &s) { return "'" + s + "'"; }

int main(int argc, char **argv)
{
	std::string command = "build", dir = ".", buildDir, prefix, jobs = "8";
	std::vector<std::string> defs;
	bool haveCommand = false;
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if (a == "-B" && i + 1 < argc) buildDir = argv[++i];
		else if (a == "--prefix" && i + 1 < argc) prefix = argv[++i];
		else if (a == "-j" && i + 1 < argc) jobs = argv[++i];
		else if (a.rfind("-D", 0) == 0) defs.push_back(a);
		else if (!haveCommand && (a == "generate" || a == "configure" || a == "build" || a == "install" || a == "test" || a == "clean" || a == "info")) { command = a; haveCommand = true; }
		else if (a == "-h" || a == "--help") { std::cout << "paradoxmake [generate|configure|build|install|test|clean|info] [-B dir] [-DNAME=VALUE] [--prefix dir] [-j n] [project-folder]\n"; return 0; }
		else dir = a;
	}
	std::map<std::string, Project> all;
	std::vector<std::string> order;
	std::set<std::string> stack;
	if (!resolve(fs::absolute(dir), all, order, stack)) return 1;
	const Project &top = all.at(order.back());
	fs::path root = top.dir.parent_path();
	fs::path build = buildDir.empty() ? root / "build" : fs::absolute(buildDir);
	fs::path gen = build / "paradoxmake";

	if (command == "info") {
		for (const auto &n : order) std::cout << n << "  [" << all.at(n).type << "]  " << all.at(n).dir.string() << "\n";
		return 0;
	}
	if (command == "clean") { fs::remove_all(build); return 0; }

	fs::create_directories(gen);
	std::string text = generate(root, all, order), old;
	{
		std::ifstream in(gen / "CMakeLists.txt");
		std::stringstream ss; ss << in.rdbuf(); old = ss.str();
	}
	if (old != text) std::ofstream(gen / "CMakeLists.txt") << text; // unchanged text leaves the timestamp alone: no needless reconfigure
	if (command == "generate") return 0;

	std::string cfg = "cmake -S " + sh(gen.string()) + " -B " + sh(build.string());
	for (const auto &d : defs) cfg += " " + sh(d);
	if (!prefix.empty()) cfg += " -DCMAKE_INSTALL_PREFIX=" + sh(prefix);
	if (int r = run(cfg)) return r;
	if (command == "configure") return 0;
	if (command == "build") return run("cmake --build " + sh(build.string()) + " -j " + jobs);
	if (command == "install") return run("cmake --build " + sh(build.string()) + " -j " + jobs) ? 1 : run("cmake --install " + sh(build.string()));
	if (command == "test") return run("ctest --test-dir " + sh(build.string()) + " --output-on-failure");
	return 0;
}

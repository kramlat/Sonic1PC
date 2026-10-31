#include <libgen.h>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>

using namespace std;

std::string format_symbol(std::string s);

std::string format_obj(std::string s);

std::string format_symbol(std::string s)
{
    std::string fs;

    for (int i = 0; i < s.length(); i++) {
        switch (s.at(i)) {
        case 'a':
            fs.push_back('A');
            break;
        case 'b':
            fs.push_back('B');
            break;
        case 'c':
            fs.push_back('C');
            break;
        case 'd':
            fs.push_back('D');
            break;
        case 'e':
            fs.push_back('E');
            break;
        case 'f':
            fs.push_back('F');
            break;
        case 'g':
            fs.push_back('G');
            break;
        case 'h':
            fs.push_back('H');
            break;
        case 'i':
            fs.push_back('I');
            break;
        case 'j':
            fs.push_back('J');
            break;
        case 'k':
            fs.push_back('K');
            break;
        case 'l':
            fs.push_back('L');
            break;
        case 'm':
            fs.push_back('M');
            break;
        case 'n':
            fs.push_back('N');
            break;
        case 'o':
            fs.push_back('O');
            break;
        case 'p':
            fs.push_back('P');
            break;
        case 'q':
            fs.push_back('Q');
            break;
        case 'r':
            fs.push_back('R');
            break;
        case 's':
            fs.push_back('S');
            break;
        case 't':
            fs.push_back('T');
            break;
        case 'u':
            fs.push_back('U');
            break;
        case 'v':
            fs.push_back('V');
            break;
        case 'w':
            fs.push_back('W');
            break;
        case 'x':
            fs.push_back('X');
            break;
        case 'y':
            fs.push_back('Y');
            break;
        case 'z':
            fs.push_back('Z');
            break;
        case '\\':
        case ' ':
        case '!':
        case '@':
        case '#':
        case '$':
        case '%':
        case '^':
        case '&':
        case '*':
        case '(':
        case ')':
        case '-':
        case '+':
        case '=':
        case '{':
        case '}':
        case '[':
        case ']':
        case '<':
        case '>':
        case '|':
        case '\"':
        case '\'':
        case ':':
        case ';':
        case '\t':
        case '\n':
        case '/':
            fs.push_back('_');
            break;
        default:
            fs.push_back(s.at(i));
            break;
        }
    }
    return fs;
}

std::string format_obj(std::string s)
{
    std::string fo;

    for (int i = 0; i < s.length(); i++) {
        switch (s.at(i)) {
        case '\\':
        case ' ':
        case '!':
        case '@':
        case '#':
        case '$':
        case '%':
        case '^':
        case '&':
        case '*':
        case '(':
        case ')':
        case '-':
        case '+':
        case '=':
        case '{':
        case '}':
        case '[':
        case ']':
        case '<':
        case '>':
        case '|':
        case '\"':
        case '\'':
        case ':':
        case ';':
        case '\t':
        case '\n':
        case '/':
            fo.push_back('_');
            break;
        default:
            fo.push_back(s.at(i));
            break;
        }
    }
    return fo;
}

/* Sonic 2's sprite mappings (a table of frame offsets, then per frame a word piece count and 8-byte pieces: y and size, tile, the 2-player tile, x) to Sonic 3 & Knuckles' (6-byte pieces without the
   2-player tile): `bin2h in out pad mappings-s3`. A frame the table names more than once is written once. Returns the new size; the input is left alone when it does not look like a table. */
static unsigned char* mappings_s2_to_s3(const unsigned char* in, long size, long* out_size)
{
    if (size < 2)
        return NULL;
    long frames = ((in[0] << 8) | in[1]) / 2;
    if (frames < 1 || frames * 2 > size)
        return NULL;
    std::vector<long> new_offset(size, -1);
    std::vector<unsigned char> out(frames * 2, 0);
    for (long f = 0; f < frames; f++) {
        long old = (in[f * 2] << 8) | in[f * 2 + 1];
        if (old + 2 > size)
            return NULL;
        if (new_offset[old] < 0) {
            long count = (in[old] << 8) | in[old + 1];
            if (old + 2 + count * 8 > size)
                return NULL;
            new_offset[old] = (long)out.size();
            out.push_back(in[old]);
            out.push_back(in[old + 1]);
            for (long p = 0; p < count; p++) {
                const unsigned char* piece = in + old + 2 + p * 8;
                out.push_back(piece[0]); out.push_back(piece[1]); // y, size
                out.push_back(piece[2]); out.push_back(piece[3]); // tile
                out.push_back(piece[6]); out.push_back(piece[7]); // x (the 2-player tile, piece[4..5], is left out)
            }
        }
        out[f * 2] = (unsigned char)(new_offset[old] >> 8);
        out[f * 2 + 1] = (unsigned char)new_offset[old];
    }
    unsigned char* result = (unsigned char*)malloc(out.size());
    memcpy(result, out.data(), out.size());
    *out_size = (long)out.size();
    return result;
}

int main(int argc, char* argv[])
{
    int result = 1;

    if (argc > 2) {
        FILE* in_file = fopen(argv[1], "rb");
        FILE* out_file = fopen(argv[2], "w");
        long pad_bytes = (argc > 3) ? atol(argv[3]) : 0;

        if (in_file == NULL) {
            printf("Couldn't open '%s'\n", argv[1]);
        } else if (out_file == NULL) {
            printf("Couldn't open '%s'\n", argv[2]);
        } else {
            long in_file_size;
            unsigned char* in_file_buffer;
            unsigned char* in_file_pointer;
            std::string str_file_path, str_file_path_fn;
            char* base = basename(argv[1]);
            str_file_path.append(argv[1]);
            str_file_path_fn.append(basename(dirname(argv[1])));
            str_file_path_fn.append("_");
            str_file_path_fn.append(base);
            std::string h_def = format_symbol(str_file_path),
                        obj_def = format_obj(str_file_path_fn);
            long i;

            fseek(in_file, 0, SEEK_END);
            in_file_size = ftell(in_file);
            rewind(in_file);
            in_file_buffer = (unsigned char*)malloc(in_file_size);
            if (fread(in_file_buffer, 1, in_file_size, in_file) < in_file_size) {
                printf("Couldn't read '%s'\n", argv[1]);
                fclose(in_file);
                return -1;
            }
            fclose(in_file);
            if (argc > 4 && std::string(argv[4]) == "mappings-s3") {
                long converted_size = 0;
                unsigned char* converted = mappings_s2_to_s3(in_file_buffer, in_file_size, &converted_size);
                if (converted == NULL) {
                    printf("'%s' is not a table of Sonic 2 sprite mappings\n", argv[1]);
                    return -1;
                }
                free(in_file_buffer);
                in_file_buffer = converted;
                in_file_size = converted_size;
            }
            in_file_pointer = in_file_buffer;

            setvbuf(out_file, NULL, _IOFBF, 0x10000);

            /* create double define protection */
            fprintf(out_file, "#ifndef %s_\n#define %s_\n", h_def.data(), h_def.data());

            /* define object */
            fprintf(out_file, "const uint8_t %s[] = {\n\t", obj_def.data());

            for (i = 0; i < in_file_size - 1; ++i) {
                if (i % 16 == 16 - 1)
                    fprintf(out_file, "%d,\n\t", *in_file_pointer++);
                else
                    fprintf(out_file, "%d,", *in_file_pointer++);
            }

            if (pad_bytes > 0) {
                fprintf(out_file, "%d,\n\t", *in_file_pointer++);
                for (long p = 0; p < pad_bytes - 1; ++p)
                    fprintf(out_file, "0,");
                fprintf(out_file, "0\n};\n#endif //%s\n\n", h_def.data());
            } else {
                fprintf(out_file, "%d\n};\n#endif //%s\n\n", *in_file_pointer++, h_def.data());
            }

            fclose(out_file);
            free(in_file_buffer);
            result = 0;
        }
    }

    return result;
}

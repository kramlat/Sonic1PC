#include <libgen.h>
#include <string>

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

int main(int argc, char* argv[])
{
    int result = 1;

    if (argc > 2) {
        FILE* in_file = fopen(argv[1], "rb");
        FILE* out_file = fopen(argv[2], "w");

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

            fprintf(out_file, "%d\n};\n#endif //%s\n\n", *in_file_pointer++, h_def.data());

            fclose(out_file);
            free(in_file_buffer);
            result = 0;
        }
    }

    return result;
}

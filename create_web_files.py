#      *****  create_web_files  *****

import sys
import argparse
import os

parser = argparse.ArgumentParser(
                    prog='create_web_files',
                    description='Generate C++ module of static strings for file contents',
                    epilog='')
parser.add_argument("-o", nargs='?', type=argparse.FileType('w'), default=sys.stdout)
parser.add_argument("files", nargs="*", type=argparse.FileType('r'))
p = parser.parse_args(sys.argv[1:])
#print(p)

p.o.writelines('#include "web_files.h"\n')
p.o.writelines('#include <stdint.h>\n')
p.o.writelines('#include <string.h>\n')
p.o.writelines('#include <string>\n')
p.o.writelines('\nWEB_FILES *WEB_FILES::singleton_ = nullptr;\n')
p.o.writelines('\nWEB_FILES::WEB_FILES()\n{\n')

fno = 1;
for file in p.files:
    p.o.writelines("    // " + os.path.basename(file.name) + "\n")
    p.o.writelines("    static char file" + str(fno) + "[] =\n")
    ty = os.path.splitext(file.name)[1]
    if ty == ".js":
        ty = ".javascript"
    p.o.writelines("        \"HTTP/1.0 200 OK\\r\\nContent-type: text/" + ty[1:] + "\\r\\n\\r\\n\"\n")
    for l in file:
        p.o.writelines("        \"" + l.rstrip() + "\\n\"\n")
    p.o.writelines("        \"\";\n\n")
    fno += 1

fno = 1;
for file in p.files:
    p.o.writelines('    files_["' + os.path.basename(file.name) + '"] = file' + str(fno) + ';\n')
    fno += 1;

p.o.writelines('}\n\n')
p.o.writelines('bool WEB_FILES::get_file(const std::string &name, const char * &data, uint16_t &datalen)\n{\n')
p.o.writelines('    auto it = files_.find(name);\n')
p.o.writelines('    if (it != files_.end())\n')
p.o.writelines('    {\n')
p.o.writelines('        data = it->second;\n')
p.o.writelines('        datalen = strlen(it->second);\n')
p.o.writelines('        return true;\n')
p.o.writelines('    }\n')
p.o.writelines('    return false;\n')
p.o.writelines('}\n')


#      *****  create_web_files  *****

import sys
import argparse
import os
import mimetypes

def escape_bytes(bytes_obj):
    return ''.join('\\n' if b == 0x0a
                   else '\\r' if b ==0x0d
                   else '\\"' if b == 0x22 
                   else '\\x{:02x}'.format(b) if b < 0x20
                   else chr(b)
                   for b in bytes_obj)

parser = argparse.ArgumentParser(
                    prog='create_web_files',
                    description='Generate C++ module of static strings for file contents',
                    epilog='')
parser.add_argument("-o", nargs='?', type=argparse.FileType('w'), default=sys.stdout)
parser.add_argument("files", nargs="*", type=argparse.FileType('rb'))
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

    mime = mimetypes.guess_type(file.name)[0]
    ms = mime.split('/')
    hdr = b"HTTP/1.0 200 OK\r\nContent-type: " + mime.encode() + b"\r\n\r\n"
    if ms[0] == 'text' or 'xml' in ms[1] or ms[1] == 'pem-certificate-chain':
        p.o.writelines("        \"" + escape_bytes(hdr) + "\"\n")
        while True:
            d = file.readline()
            if not d:
                break
            s = escape_bytes(d)
            p.o.writelines("        \"" + s + "\"\n")
        p.o.writelines("        \"\";\n\n")
    else:
        p.o.writelines("    {\n")
        p.o.writelines("    ")
        for c in hdr:
            p.o.writelines(repr(chr(c)) + ',')
            if c == 0x0a:
                p.o.writelines("\n    ")

        while True:
            d = file.read(20)
            if not d:
                break
            for c in d:
                p.o.writelines('0x{:02x}'.format(c) + ',')
            p.o.writelines("\n    ")
            
        p.o.writelines("0};\n")

    fno += 1

fno = 1;
for file in p.files:
    p.o.writelines('    files_["' + os.path.basename(file.name) +
                   '"] = std::pair<const char *, int>(file' + str(fno) + ', sizeof(file' + str(fno) + ') - 1);\n')
    fno += 1;

p.o.writelines('}\n\n')
p.o.writelines('bool WEB_FILES::get_file(const std::string &name, const char * &data, uint16_t &datalen)\n{\n')
p.o.writelines('    auto it = files_.find(name);\n')
p.o.writelines('    if (it != files_.end())\n')
p.o.writelines('    {\n')
p.o.writelines('        data = it->second.first;\n')
p.o.writelines('        datalen = it->second.second;\n')
p.o.writelines('        return true;\n')
p.o.writelines('    }\n')
p.o.writelines('    return false;\n')
p.o.writelines('}\n')


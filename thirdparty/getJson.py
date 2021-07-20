import sys
import shutil
if sys.version_info < (3, 0):
    sys.stdout.write("Sorry, requires Python 3.x, not Python 2.x\n")
    sys.exit(1)

import tarfile
import os 
import urllib.request
import subprocess
import platform


def getJson(install, prefix, par):
    
    osStr = (platform.system())
    cwd = os.getcwd()
    folder = cwd + "/json"
    url = "https://github.com/nlohmann/json.git  "

    if os.path.exists(folder) == False:
        os.system("git clone " + url)
    os.chdir(folder)
    #os.system("git checkout 3a0746bf5f601dfed05330aefcb6854354fce07d")

    sudo = ""
    if(osStr == "Windows"):
        
        if install and len(prefix) == 0:
            print("no default install location on windows")
            exit()

        if len(prefix) == 0:
            prefix = cwd + "/win"

        InstallCmd = "cp ./include/nlohmann " + prefix + "/include/nlohmann -Recurse -Force" 

        temp = "./buildFunction2_deleteMe.ps1"
        f2 = open(temp, 'w')
        f2.write(InstallCmd)
        f2.close()

        print("\n=========== getJson.py =============")
        print(InstallCmd)
        print("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n")

        p = subprocess.Popen(['powershell.exe', temp])
        p .communicate()

        os.remove(temp)

    else:
        if install and "--sudo" in sys.argv:
            sudo = "sudo "

        if not install and len(prefix) == 0:
            prefix = cwd + "/unix"
        
        if install and len(prefix) == 0:
            prefix = "/usr/local"
        
        mkdirCmd = sudo + "mkdir -p " + prefix + "/include/nlohmann/"
        InstallCmd = sudo + "cp -rf ./include/nlohmann " + prefix + "/include" 

        print("\n=========== getJson.py =============")
        print(mkdirCmd)
        print(InstallCmd)        
        print("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n")

        os.system(mkdirCmd)
        os.system(InstallCmd)




    os.chdir(cwd)


if __name__ == "__main__":
    getJson(False, "", 1)
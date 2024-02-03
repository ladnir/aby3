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


def getFunction2(install, prefix, par):
    
    osStr = (platform.system())
    cwd = os.getcwd()
    folder = cwd + "/function2"
    url = "https://github.com/Naios/function2.git"
    function2_tag = "4.1.0"

    if os.path.exists(folder) == False:
        subprocess.run(f"git clone --depth 1 --branch {function2_tag} {url}".split(), check=True)
    os.chdir(folder)

    sudo = ""
    if(osStr == "Windows"):
        
        if install and len(prefix) == 0:
            print("no default install location on windows")
            exit()

        if len(prefix) == 0:
            prefix = cwd + "/win"

        InstallCmd = "cp ./include/function2 " + prefix + "/include/function2 -Recurse -Force" 

        temp = "./buildFunction2_deleteMe.ps1"
        f2 = open(temp, 'w')
        f2.write(InstallCmd)
        f2.close()

        print("\n=========== getFunction2.py =============")
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

        mkdirCmd = (sudo + "mkdir -p " + prefix + "/include/function2/").split()
        InstallCmd = (sudo + "cp  -rf ./include/function2 " + prefix + "/include").split()

        print("\n=========== getFunction2.py =============")
        print(mkdirCmd)
        print(InstallCmd)
        print("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n")

        subprocess.run(mkdirCmd, check=True)
        subprocess.run(InstallCmd, check=True)

        #buildDir = "out/unix"
        
        #CMakeCmd = "cmake -S . -B {0} ".format(buildDir)
        #BuildCmd = "cmake --build {0} ".format(buildDir)
        #InstallCmd = sudo +"cmake --install {0} ".format(buildDir)
        #if len(prefix):
        #    InstallCmd += " --prefix {0} ".format(prefix)

        #print("\n=========== getEigen.py =============")
        #print(CMakeCmd)
        #print(BuildCmd)
        #print(InstallCmd)
        #print("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n")

        #os.system(CMakeCmd)
        #os.system(BuildCmd)

        #if len(sudo) > 0:
        #    print("installing libraries: {0}".format(InstallCmd))
        #os.system(InstallCmd)



    os.chdir(cwd)


if __name__ == "__main__":
    getFunction2(False, "", 1)
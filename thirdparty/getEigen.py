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


def getEigen(install, prefix, par):
    
    folder = "eigen-3.3.4"
    arch = "eigen-3.3.4.tar.bz2"
    url = "https://gitlab.com/libeigen/eigen/-/archive/3.3.4/eigen-3.3.4.tar.bz2"

    cwd = os.getcwd()

    
    if os.path.isdir(folder) == False:
        if not os.path.exists(arch):
            try:
                print("url: ", url)
                print("downloading eigen...")
                urllib.request.urlretrieve(url, arch)
            except:
                print("failed to download eigen. please manually download the archive to")
                print("{0}/{1}".format(cwd, arch))

        print("extracting eigen...")
        tar = tarfile.open(arch, 'r:bz2')
        tar.extractall()
        tar.close()
        os.remove(arch)

    os.chdir(cwd + "/" + folder)

    sudo = ""
    buildDir =""
    osStr = (platform.system())
    if(osStr == "Windows"):
        
        if install and len(prefix) == 0:
            print("no default install location on windows")
            exit()

        if len(prefix) == 0:
            prefix = cwd + "/win"

        InstallCmd = "cp ./Eigen " + prefix + "/include/Eigen/ -Recurse -Force" 

        temp = "./buildEigen_deleteMe.ps1"
        f2 = open(temp, 'w')
        f2.write(InstallCmd)
        f2.close()

        print("\n=========== getEigen.py =============")
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

        mkdirCmd = sudo + "mkdir -p " + prefix + "/include/Eigen/"
        InstallCmd = sudo + "cp  -rf ./Eigen " + prefix + "/include" 

        print("\n=========== getEigen.py =============")
        print(mkdirCmd)
        print(InstallCmd)
        print("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n")

        os.system(mkdirCmd)
        os.system(InstallCmd)

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
    getEigen(False, "", 1)
import os 
import sys 
import platform
import subprocess


def getLibOTe(install, prefix, par,libOTe, boost, relic):
    
    cwd = os.getcwd()
    
    if os.path.isdir("libOTe") == False:
        subprocess.run("git clone --recursive https://github.com/osu-crypto/libOTe.git".split(), check=True)

    os.chdir(cwd + "/libOTe")
    subprocess.run("git checkout cf537295c47a3924c13030a9b796cee9d6ebeace".split(), check=True)
    subprocess.run("git submodule update".split(), check=True)

    osStr = (platform.system())
    
    debug = ""    
    if "--debug" in sys.argv:
        debug = " --debug "

    sudo = ""
    if(osStr == "Windows"):
        if not install:
            prefix = cwd + "/win"
            if len(debug):
                prefix += "-debug"

    else:
        if not install:
            prefix = cwd + "/unix"
        if install and "--sudo" in sys.argv:
            sudo = "--sudo "



    installCmd = ""
    if len(prefix) > 0:
        installCmd = "--install=" + prefix
    elif install:
        installCmd = "--install"

    cmakePrefix = ""
    if len(prefix) > 0:
        cmakePrefix = "-DCMAKE_PREFIX_PATH=" + prefix

    cmd =  "python3 build.py " + sudo + " --par=" + str(par) + " " + installCmd + " " + debug
    boostCmd = (cmd + " --setup --boost ").split()
    relicCmd = (cmd + " --setup --relic ").split()
    libOTeCmd = (cmd + " -DENABLE_CIRCUITS=ON " + cmakePrefix).split()
    
    
    print("\n\n=========== getLibOTe.py ================")
    if boost:
        print(boostCmd)
    if relic:
        print(relicCmd)
    if libOTe:
        print(libOTeCmd)
    print("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n\n")

    if boost:
        subprocess.run(boostCmd, check=True)
    if relic:
        subprocess.run(relicCmd, check=True)
    if libOTe:
        subprocess.run(libOTeCmd, check=True)

    os.chdir(cwd)
    
if __name__ == "__main__":
    getLibOTe(False, "", 1, True, True, True)
import os 
import sys 
import platform


def getLibOTe(install, prefix, par,libOTe, boost, relic):
    
    cwd = os.getcwd()
    
    if os.path.isdir("libOTe") == False:
        os.system("git clone --recursive https://github.com/osu-crypto/libOTe.git")

    os.chdir(cwd + "/libOTe")
    os.system("git checkout 2363505431f744539027a873c2536b9ae3630ff7 --quiet ")
    os.system("git submodule update ")

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
    boostCmd = cmd + " --setup --boost "
    relicCmd = cmd + " --setup --relic "
    libOTeCmd = cmd + " -DENABLE_CIRCUITS=ON " + cmakePrefix;
    
    
    print("\n\n=========== getLibOTe.py ================")
    if boost:
        print(boostCmd)
    if relic:
        print(relicCmd)
    if libOTe:
        print(libOTeCmd)
    print("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n\n")

    if boost:
        os.system(boostCmd)
    if relic:
        os.system(relicCmd)
    if libOTe:
        os.system(libOTeCmd)

    os.chdir(cwd)
    
if __name__ == "__main__":
    getLibOTe(False, "", 1, True, True, True)
import os
import platform
import sys
import multiprocessing

if __name__ == "__main__":
    import thirdparty.getEigen as getEigen
    import thirdparty.getJson as getJson
    import thirdparty.getFunction2 as getFunction2
    import thirdparty.getLibOTe as getLibOTe
else:
    from .thirdparty import getSparsehash
    from .thirdparty import getLibOTe

#import thirdparty

def getParallel(args):
    par = multiprocessing.cpu_count()
    for x in args:
        if x.startswith("--par="):
            val = x.split("=",1)[1]
            par = int(val)
            if par < 1:
                par = 1
    return par


def Setup(boost, libOTe,eigen, json,function2, install, prefix, par):
    dir_path = os.path.dirname(os.path.realpath(__file__))
    os.chdir(dir_path + "/thirdparty")

    if not libOTe and  not eigen and not json and not function2 and not boost:
        sparsehash = True
        libOTe = True
        boost = True
        eigen = True
        json = True
        function2 = True

    if boost  or libOTe:
        getLibOTe.getLibOTe(install, prefix, par, libOTe, boost, False)
    if function2:
        getFunction2.getFunction2(install, prefix, par)
    if eigen:
        getEigen.getEigen(install, prefix, par)
    if json:
        getJson.getJson(install, prefix, par)

    os.chdir(dir_path)


def Build(mainArgs, cmakeArgs,install, prefix, par):

    osStr = (platform.system())
    buildDir = ""
    config = ""
    buildType = ""
    if "--Debug" in mainArgs:
        buildType = "Debug"
    else:
        buildType = "Release"

    if osStr == "Windows":
        buildDir = "out/build/x64-{0}".format(buildType)
        config = "--config {0}".format(buildType)
    else:
        buildDir = "out/build/linux"

    cmakeArgs.append("-DCMAKE_BUILD_TYPE={0}".format(buildType))

    argStr = ""
    for a in cmakeArgs:
        argStr = argStr + " " + a

    parallel = ""
    if par != 1:
        parallel = " --parallel " + str(par)



    mkDirCmd = "mkdir -p {0}".format(buildDir); 
    CMakeCmd = "cmake -S . -B {0} {1}".format(buildDir, argStr)
    BuildCmd = "cmake --build {0} {1} {2} ".format(buildDir, config, parallel)


    #InstallCmd = ""
    #sudo = ""
    #if "--sudo" in sys.argv:
    #    sudo = "sudo "

    #if install:
    #    InstallCmd = sudo
    #    InstallCmd += "cmake --install {0} {1} ".format(buildDir, config)

    #    if len(prefix):
    #        InstallCmd += " --prefix {0} ".format(prefix)

    projectName = "aby3"
    print("======= build.py ("+projectName+") ==========")
    print(mkDirCmd)
    print(CMakeCmd)
    print(BuildCmd)
    #if len(InstallCmd):
    #    print(InstallCmd)
    print("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n")

    os.system(mkDirCmd)
    os.system(CMakeCmd)
    os.system(BuildCmd)
    #if len(sudo) > 0:
    #    print("installing libraries: {0}".format(InstallCmd))
    #
    #os.system(InstallCmd)


def getInstallArgs(args):
    prefix = ""
    for x in args:
        if x.startswith("--install="):
            prefix = x.split("=",1)[1]
            prefix = os.path.abspath(os.path.expanduser(prefix))
            return (True, prefix)
        if x == "--install":
            return (True, "")
    return (False, "")


def parseArgs():
    
    
    hasCmakeArgs = "--" in sys.argv
    mainArgs = []
    cmakeArgs = []

    if hasCmakeArgs:
        idx = sys.argv.index("--")
        mainArgs = sys.argv[:idx]
        cmakeArgs = sys.argv[idx+1:]

    else:
        mainArgs = sys.argv


    return (mainArgs, cmakeArgs)

def help():
    print(" --setup    \n\tfetch, build and optionally install the dependencies. \
    Must also pass --libOTe, --relic and/or --boost to specify which to build. Without \
    --setup, the main library is built.")

    print(" --install \n\tInstructs the script to install whatever is currently being built to the default location.")
    print(" --install=prefix  \n\tinstall to the provided predix.")
    print(" --sudo  \n\twhen installing, use sudo. May require password.")
    print(" --par=n  \n\twhen building do use parallel  builds with n threads. default = num cores.")
    print(" --  \n\tafter the \"--\" argument, all command line args are passed to cmake")

    print("\n\nExamples:")
    print("-fetch the dependancies and dont install")
    print("     python build.py --setup --boost --relic")
    print("-fetch the dependancies and install with sudo")
    print("     python build.py --setup --boost --relic --install --sudo")
    print("-fetch the dependancies and install to a specified location")
    print("     python build.py --setup --boost --relic --install=~/my/install/dir")
    print("")
    print("-build the main library")
    print("     python build.py")
    print("-build the main library with cmake configurations")
    print("     python build.py -- -DCMAKE_BUILD_TYPE=Debug -DENABLE_SSE=ON")
    print("-build the main library and install with sudo")
    print("     python build.py --install --sudo")
    print("-build the main library and install to prefix")
    print("     python build.py --install=~/my/install/dir ")




def main():
    (mainArgs, cmake) = parseArgs()
    if "--help" in mainArgs:
        help()
        return 

    boost = ("--boost" in mainArgs)
    eigen = ("--eigen" in mainArgs)
    json = ("--json" in mainArgs)
    function2 = ("--function2" in mainArgs)
    libOTe = ("--libOTe" in mainArgs)
    setup = ("--setup" in mainArgs)
    install, prefix = getInstallArgs(mainArgs)
    par = getParallel(mainArgs)

    if(setup):
        Setup(boost, libOTe,eigen, json,function2,install, prefix, par)
    else:
        Build(mainArgs, cmake,install, prefix, par)

if __name__ == "__main__":

    main()

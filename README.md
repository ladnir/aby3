# ABY 3 and Applications
 
## Introduction
 
This library provides the semi-honest implementation of [ABY 3](https://eprint.iacr.org/2018/403.pdf) and [Fast Database Joins for Secret Shared Data](https://eprint.iacr.org/2019/518.pdf).

The repo includes the following application:
 * Linear Regression (training/inference)
 * Logistic Regression (training/inference)
 * Database Inner, Left and Full Joins
 * Database Union
 * Set Cardinality
 * Threat Log Comparison ([see Section 5](https://eprint.iacr.org/2019/518.pdf))
 * ERIC Application ([see Section 5](https://eprint.iacr.org/2019/518.pdf))


Note that this is *not* the same implementation provided in the publication [ABY3: A Mixed Protocol Framework for Machine Learning](https://eprint.iacr.org/2018/403.pdf), however it is very similar. Basicly this a much more usable reimplementation by me that was done for the database paper. If you wish to have the original ABY3 implementation, please contact me.

## Warning 

This codebase should **NOT** be considered fully secure. It has not had a security review and there are still several security related issues that have not been fully implemented. Only use this casebase as a proof-of-concept or to benchmark the perfromance. Future work is required for this implementation to be considered secure. 

## Install
 
The library is *cross platform* and has been tested on Windows and Linux. The dependencies are:

 * [libOTe](https://github.com/osu-crypto/libOTe)
 * [Boost](http://www.boost.org/) (networking)
 * [function2](https://github.com/Naios/function2)
 * [Eigen](http://eigen.tuxfamily.org/index.php?title=Main_Page)

### Windows

 1) Clone and build [libOTe](https://github.com/osu-crypto/libOTe). 
 2) Edit `libOTe/cryptoTools/cryptoTools/Common/config.h` to contain `#define ENABLE_CIRCUITS ON`. 
 3) `git clone https://github.com/ladnir/aby3.git`
 4) `cd C:\`
 5) `mkdir libs`
 6) Download [Eigen](http://eigen.tuxfamily.org/index.php?title=Main_Page) to `C:\libs\eigen`.
 7) `git clone https://github.com/Naios/function2.git`  to `C:\libs\function2`.

Open `aby3.sln` in visual studio and build it. Note, you can place Eigen and function2 in a different location and update the aby3 project files to reference this location.

To see all the command line options, execute the program 

`frontend.exe` 


### Linux / Mac
 
In short, this will build the project

```
git clone --recursive https://github.com/osu-crypto/libOTe.git
cd libOTe/cryptoTools/thirdparty/linux
bash boost.get
cd ../../..
cmake . -DENABLE_CIRCUITS=ON
make -j
cd ../
git clone https://github.com/ladnir/aby3.git
cd thirdparty/linux
bash eigen.get
bash function2.get
cd ../..
cmake .
make -j
```

To see all the command line options, execute the program 
 
`./bin/frontend`


### Linking

 To use the library in your project, you will need to link the following:

1) .../libOTe
2) .../libOTe/cryptoTools
3) .../libOTe/cryptoTools/thirdparty/linux/boost
4) .../aby3

and link:
1) .../libOTe/bin/liblibOTe.a
2) .../libOTe/bin/libcryptoTools.a
3) .../aby3/bin/libaby3.a
4) .../libOTe/cryptoTools/thirdparty/linux/boost/stage/lib/libboost_system.a
5) .../libOTe/cryptoTools/thirdparty/linux/boost/stage/lib/libboost_thread.a


**Note:** On windows the linking paths follow a similar pattern.

## Help
 
Contact Peter Rindal peterrindal@gmail.com for any assistance on building  or running the library.

## Citing

 Spread the word!

```
@misc{aby3,
    author = {Peter Rindal},
    title = {{The ABY3 Framework for Machine Learning and Database Operations.}},
    howpublished = {\url{https://github.com/ladnir/aby3}},
}
```

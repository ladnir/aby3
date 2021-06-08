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

A tutorial can be found [here](https://github.com/ladnir/aby3/blob/master/frontend/aby3Tutorial.cpp). It includes a description of how to use the API and a discussion at the end on how the framework is implemented.

## Warning 

This codebase should **NOT** be considered fully secure. It has not had a security review and there are still several security related issues that have not been fully implemented. Only use this casebase as a proof-of-concept or to benchmark the perfromance. Future work is required for this implementation to be considered secure. 

Moreover, some features have not been fully developed and contains bugs. For example, the task scheduler sometime fails. This is a known issue.

## Install
 
The library is *cross platform* and has been tested on Windows and Linux. The dependencies are:

 * [libOTe](https://github.com/osu-crypto/libOTe)
 * [Boost](http://www.boost.org/) (networking)
 * [function2](https://github.com/Naios/function2)
 * [Eigen](http://eigen.tuxfamily.org/index.php?title=Main_Page)

### Windows

 1) Clone and build [libOTe](https://github.com/osu-crypto/libOTe) via `cmake . -DENABLE_CIRCUITS=ON`. 
 2) `git clone https://github.com/ladnir/aby3.git`
 3) `cd C:\`
 4) `mkdir libs`
 5) Download [Eigen](http://eigen.tuxfamily.org/index.php?title=Main_Page) to `C:\libs\eigen`.
 6) `git clone https://github.com/Naios/function2.git`  to `C:\libs\function2`.

Open `aby3` folder in visual studio and build it (via cmake). Note, you can place Eigen and function2 in a different location and update the aby3 cmake files to reference this location.

To see all the command line options, execute the program 

`frontend` 


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
cd aby3/thirdparty/linux
bash eigen.get
bash function2.get
cd ../..
cmake .
make -j
```

To see all the command line options, execute the program 
 
`frontend`

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

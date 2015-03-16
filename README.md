# Pipeline Merge Sort
This is an implementation of Pipeline Merge Sort algorithm using OpenMPI parallel computing framework.

## Description of the algorithm
http://goo.gl/2LRMhl

## Dependencies
* C++ compiler with C++11 standard support
* OpenMPI (tested with openmpi-1.6.4)
* Ruby 2.0 for running test

## Description
After you check out git repo, you can try running `make` command. If you are lucky and compilation doesn't fail, then
please use attached script `test.sh` to run the program. Script expects exactly one argument, a size of the input. It
should be power of two, otherwise next bigger power of 2 will be used instead. Script will then generate random of
given size and run the algorithm with required number of processors, which is log_2(input_size) + 1.

## Usage
1. `git clone https://github.com/msekletar/pms.git`
2. `cd pms`
3. `make`
4. `./test.sh <input_size>`

## License note
Surce code is provided under terms of GPLv2 license. Note that this project was developed as part of
the course work at Brno University of Technology. Therefore if you are BUT student then most likely
you are not allowed use the source code, because of the universtity collaboration policy. In any case
you should speak to your professor before using source code from this repository.

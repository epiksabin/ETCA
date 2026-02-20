# can be ran from ..\build.bat
# auto build for this project
# ensure you have cmake, clang++ and minGW installed

cd build
cmake -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=clang++ ..
cmake --build .

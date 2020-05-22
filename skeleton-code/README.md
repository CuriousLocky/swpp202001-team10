# How to add a new pass

Basically to add a new pass so that our `main.cpp` can use it, you need to do
the following things.

1. Create a header file under `/include`

   Your pass is a class using template of  `PassInfoMixin<>`, and you need to 
   declare it so that other's code can use the class you created. Thus, you need
   to add a header file under `/include`. The skeleton code is given as 
   `/include/Example.h` and there you can see a public `run` function. You can 
   add more private function declarations if you need them.

   The naming of the class should follow upper camel case and the functions
   should follow lower camel case.

2. Create a source file under `/src`

   After declaration, you need to implement the class you declare. You can find
   skeleton code at `/src/Example.cpp`.

   If you want to add helper functions, other than declare them as private
   methods, you can add them as static functions in your source file. We don't
   have any restrictions here.

3. Add your pass to `main.cpp`

   The next thing you want to do is to use the pass you created in `main.cpp`.
   Skeleton code is provided as `/src/main.cpp`

4. Modify `Makefile.template` and regenerate `Makefile`

   The code is added, but when you simply type `make`, you can either get an
   error or find that your pass is not effective (or maybe something else). The
   mechanism of `Makefile` will not be discussed here. What you need to do is to
   go to `Makefile.template`, and add a line there. Example can be found in 
   `Makefile.template` under this directory.

   After that, you need to either regenerate `Makefile` or edit your existing 
   `Makefile` to make your code compiled and linked correctly. The process is
   the same as what you did to `Makefile.template` if you want to do the later.

5. Do everything else normally
![HyperCalc Logo](https://raw.githubusercontent.com/hyper-prog/hypercalc/master/hypercalc_small.png)


HyperCalc - High precision command line styled calculator
=========================================================

HyperCalc is a console styled calculator, which is similar to linux "bc".
It designed to be simple, easy to use and fast. The calculations evaluated with correct precedence, 
using decimal based numbers with high precision to avoid floating point errors.
(The base mathematical functions built on boost c++)
You can use hexadecimal, octal and binary numbers as input or output.
Available operators/sings:

	+ - * / % ^ ( )

The program has many internal functions and loadable functions and you can define new ones (See screenshots for examples)<br/>

![HyperCalc screenshot 1](https://raw.githubusercontent.com/hyper-prog/hypercalc/master/images/hypercalc_examples.png)

![HyperCalc screenshot 1](https://raw.githubusercontent.com/hyper-prog/hypercalc/master/images/hypercalc_scrn1.png)

Compile / Install
-----------------

Require gSAFE (Qt based opensource lib) and Boost C++.
Check the paths of these libraries in HyperCalc.pro file the run:

    $qmake
    $make
    #make install

Author
------
 Péter Deák (hyper80@gmail.com)

License
-------
 GPLv2

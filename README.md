# The V8 wrapper for svar_modules.

[Svar](https://github.com/zdzhaoyong/Svar) is a project aims to unify c++ interfaces for all languages. 
Now we are able to use svar_modules in some languages such as python.
This project is use to provide javascript support.

**Warning**: This project is still under develop.

svar_v8 auto wrap the following c++ objects:

* Json Types: number, boolean, string, object, array
* Function  : 
* C++ Classes: Auto wrap constructors, member functions
* C++ Objects: User don't need to declare every classes, svar auto wrap them.

TODO:

* Class Properties Accessor
* Class inherit

## Compile and Install

Please install [Svar](https://github.com/zdzhaoyong/Svar) firstly.

```
cd svar_v8
npm install .
```

## Usage Demo

Write the following index.js file and run with node:

```
svar=require('./build/Release/svar.node')

sm=svar.load('sample_module')

person=new sm.Person(28,'zhaoyong')

console.log(person.intro())
```



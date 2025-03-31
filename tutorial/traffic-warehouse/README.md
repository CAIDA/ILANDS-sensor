# Traffic Warehouse

## Web Build

The web build is not able to load external learning modules, but it does have  
all of the currently created ones integrated into it. In the future, we may add  
additional integrated learning modules into the web build.

### Web Build How-to
1. Download all the contents of the builds/web directory.
2. Host a webserver that can run HTML5 pointing to that directory.
3. Navigate to your hosted webserver and enjoy!

## Windows Build
The windows build has both the integrated learning modules as well as the ability  
to load your own learning modules. To learn how to create your own learning modules,  
go to the "Learning Modules" section below.

### Windows Build How-to
1. Download all the contents of the builds/windows directory.
2. Execute TrafficWarehouse_1-0.exe
3. Enjoy!

NOTE: When you load external learning modules, remember that they need to be in  
a .zip file in order to be loaded correctly. See "Learning Modules" below.

## Learning Modules
To create your own learning modules, modify the templates in learning-modules/templates.  
Zip up all of your created lessons (the JSON files) into a zip file (even if its just one)  
and then load it from the home screen of Traffic Warehouse.

When assigning traffic matric colors, 0=grey 1=blue 2=red 3=black

When creating questions, you can only use three questions. This is based on research  
that has shown three questions to be the optimal amount in a multiple choice question.



## Acknowledgment
Everything in this directory and all subdirectories of this directory fall under  
the following agreement:

Research was sponsored by the Department of the Air Force Artificial Intelligence  
Accelerator and was accomplished under Cooperative Agreement Number FA8750-19-2-1000.  
The views and conclusions contained in this document are those of the authors and  
should not be interpreted as representing the official policies, either expressed  
or implied, of the Department of the Air Force or the U.S. Government. The U.S.  
Government is authorized to reproduce and distribute reprints for Government purposes  
notwithstanding any copyright notation herein.
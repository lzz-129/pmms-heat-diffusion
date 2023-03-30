# Assignment 3

The CLI of the heat dissipation part of the assignment is the same as for assignment 1 & 2. 

The CLI for the histogram part is as follows:
 - **-s** sets the seed (default 42)
 - **-i** image path
 - **-r** if set then generate a random image (default False)
 - **-p** sets the number of threads used (default 1)
 - **-n** number of rows of the image
 - **-m** number of columns of the image
 - **-g** debug (print historgram)

 You may change everything in the boilerplate code. The only thing that **HAS TO** stay the same is the current options in the CLI and the output this produces. So you may change things like the data structures used **BUT** we still want the same output when printing the histogram. You may also extend the functionality (i.e. if you want to generate images of the same color you can write a second "generate image" function). We will only test your code using the images in the "images" folder.  

The CLI for the pipesort is as follows:
 - **-l** sets the length of the vector
 - **-s** sets the seed of the random number generator
 You may extend the CLI, but not alter the behaviour of "l" and "s". Set any additional flags to the default you want us to use. 


## Histogram Testing: 
For correctness:
        `-i ../../../images/pat1_100x150.pgm -n 150 -m 100 -g -p 2`
        

## Pipesort Testing:
For correctness:
         `-l 4090 -s 42`
                   
If you really want to test your performance you can try even larger sequence lengths (up to say 50,000) but you may want to look at `man getrlimit` and/or increase your buffer sizes.
        
  
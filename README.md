# Parallel Low Pass Filter (Bluring)

## Description

It is used to make images appear smoother. Low pass filtering smooths out noise. It allows low frequency components of
the image to pass through and blocks high frequencies. Low pass image filters work by convolution which is a process of
making each pixel of an image by a dynamic kernel filter size.

## Idea

- Master node does not work in filtering. It takes inputs (No. of processors and filter size), divides image array,
  sends sub-arrays to other processors, gathers results and creates the new blurred image then saves it.

Working with MPI (Message Passing Interface)

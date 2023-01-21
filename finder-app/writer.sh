#!/bin/bash

#Author: Geoffrey Jensen
#ECEA 5305 Assignment #1
#Date: 01/21/2023
#Description: Takes an input file name (must include full path including file
 #name) and also takes an input string that it writes to this file. Creates the
 #file if it doesn't already exist and overwrites file if it does.

writefile=$1
writestr=$2

#Check the input argument count to ensure both arguments are provided
if [ $# != 2 ]
then
        echo "Incorrect number of input arguments provided." 
        echo "Please ensure you use the following format:"
        echo "./writer.sh FileName TextString"
	echo "Argument #1 (FileName) must be the full path to a file and include the filename"
	echo "Argument #2 (TextString) is a string that will be written to that file"
        exit 1
fi

#Implementation of writing input string to a file. Check if directory exists and if not, make the directory path first. Then regardless just write contents 
directoryName=$(dirname $writefile)
if [ ! -d $directoryName ]
then
	mkdir -p $directoryName
fi
echo $writestr > $writefile

exit 0

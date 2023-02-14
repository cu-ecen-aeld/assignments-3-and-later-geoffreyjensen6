#!/bin/sh

#Author: Geoffrey Jensen
#ECEA 5305 Assignment #1
#Date: 01/21/2023
#Description: Takes an input argument for a directory and also a search string.
 #It then looks for this string in all the files in that directory and returns
 #the count of files it found in the directory and the number of matches it 
 #found with that string in those files

filesdir=$1
searchstr=$2

#Check the input argument count to ensure both arguments are provided
if [ $# != 2 ] 
then
	echo "Incorrect number of input arguments provided." 
	echo "Please ensure you use the following format:"
        echo "./finder.sh Directory SearchString"
	echo "Argument #1 (Directory) must be an existing directory"
	echo "Argument #2 (SearchString) is the string you want to search for in the files of that directory"
	exit 1
fi

#Check the input argument for the file directory to ensure that it is a valid directory
if [ ! -d $filesdir ] 
then
	echo "Unable to find directory: $filesdir."
       	echo "Please correct the input argument to an existing directory."
	exit 1
fi

#Implement search function with term and directory provided
fileNum=$(ls -l $filesdir | wc -l)
fileNum=$(expr $fileNum - 1)
matchCount=$(grep -r $searchstr $filesdir/* | wc -l)
echo "The number of files are $fileNum and the number of matching lines are $matchCount"


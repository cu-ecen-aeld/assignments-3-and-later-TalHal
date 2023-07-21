#! /bin/sh

if [ $# != 2 ]; then
	echo "parameters: [filsdir] [searchstr]"
	exit 1
fi

if [ ! -d $1 ]; then 
	echo "directory $1 doesn't exist"
	exit 1
fi

filesdir=$1

files_num=`find  ${filesdir} -type f | wc -l`

searchstr=$2 

matching_lines=`grep -rn ${searchstr} ${filesdir} | wc -l`

echo "The number of files are ${files_num} and the number of matching lines are ${matching_lines}"

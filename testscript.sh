#!/bin/bash
echo "Running 100 iterations of spooler..."
for i in {0..100}
  do
    echo "Spooler run $i:"
	./spooler
 done

#include <stdio.h>

int array[] = {4,2,3,7,5};

/*
  * 
  */
static void insert_sort(int array[], int len);

static void insert_sort(int array[], int len)
{
	int key;
	int i,j;
	for(j = 1; j < len; j++){
		i = j - 1;
		key = array[j];
		while(array[i] > key && i > 0) {
			array[i + 1] = array[i];
			i--;
		}
		array[i + 1] = key;
	}
}
int main(int argc, char *argv[])
{
	insert_sort(array);
}


#include "stl_model/stl_model.h"

int EXIT_STATUS = 0;

//----------------------------------------------------------------------------------------------------
int main(int argc, char* argv[]) 
{
	EXIT_STATUS = stl_review("insert_wall_mount_vc.stl", argc, argv);
	
	return EXIT_STATUS;
}
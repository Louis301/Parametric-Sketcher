#include "stl_model_view/stl_model.h"

int EXIT_STATUS = 0;

//----------------------------------------------------------------------------------------------------
int main(int argc, char* argv[]) 
{
	EXIT_STATUS = stl_review("base_wall_mount_vc.stl", argc, argv);
	
	return EXIT_STATUS;
}



feat/stl-to-step-converter
// #include "stl_model_view/stl_model.h"

// int EXIT_STATUS = 0;

// //----------------------------------------------------------------------------------------------------
// int main(int argc, char* argv[]) 
// {

// 	// EXIT_STATUS = stl_review("swed_house.stl", argc, argv);
// 	return EXIT_STATUS;
// }


#include <igl/readOBJ.h>
#include <igl/cotmatrix.h>
#include <Eigen/Dense>
#include <iostream>

int main() {
    Eigen::MatrixXd V;
    Eigen::MatrixXi F;
    
    // Создаём простую тестовую сетку (тетраэдр)
    V.resize(4, 3);
    V << 0, 0, 0,
         1, 0, 0,
         0, 1, 0,
         0, 0, 1;
    
    F.resize(4, 3);
    F << 0, 1, 2,
         0, 1, 3,
         0, 2, 3,
         1, 2, 3;
    
    // Тест: вычисление котангенс-матрицы
    Eigen::SparseMatrix<double> L;
    igl::cotmatrix(V, F, L);
    
    std::cout << "[Success] libigl работает! Размер матрицы: " 
              << L.rows() << "x" << L.cols() << std::endl;
    
    return 0;
}
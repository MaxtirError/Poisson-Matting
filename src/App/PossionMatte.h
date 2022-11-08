#pragma once
#include<QImage>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>
#include "ANN/ANN.h"
#include <Eigen/Sparse>
using namespace Eigen;
typedef std::vector<cv::Point2i> Boundary;
typedef SparseMatrix<double> SpMat;
typedef Triplet<double> T;
class PossionMatte 
{
public:
	PossionMatte(int H, int W, int **I, int **Trimap) : W(W), H(H), I(I), Trimap(Trimap) {}
	void Matting_Gauss();
	void Matting_Eigen();
	double** alpha;
private:
	const int dx[4] = { 0, 0, 1, -1};
	const int dy[4] = { 1, -1, 0, 0};
	ANNkd_tree* Get_Boundary(int type, Boundary &boundary, ANNpointArray &Pts);
	int **I;
	int H, W;
	int** Trimap; //0 for background, 1 for foreground , 2 for unknow, 3 for background+. 4 for foreground+
	Boundary bd_foreground, bd_background;
	ANNkd_tree *tree_bk, *tree_fg;
};
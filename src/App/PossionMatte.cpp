#include "PossionMatte.h"
ANNkd_tree* PossionMatte::Get_Boundary(int type, Boundary& boundary, ANNpointArray &Pts)
{
	boundary.clear();
	for (int i = 0; i < H; ++i)
		for (int j = 0; j < W; ++j)
			if (Trimap[i][j] == type) //omega & omega+
			{
				int d = 0;
				for (; d < 4; ++d)
				{
					int ni = i + dx[d], nj = j + dy[d];
					if (ni < 0 || ni >= H || nj < 0 || nj >= W)
						continue;
					if (Trimap[ni][nj] == 2)
						break;
				}
				if (d != 4)
					boundary.push_back(cv::Point2i(i, j));
			}
	int sz = boundary.size();
	std::cout << sz << std::endl;
	Pts = annAllocPts(sz, 2);
	for (int i = 0; i < sz; ++i)
		Pts[i][0] = boundary[i].x, Pts[i][1] = boundary[i].y;
	return new ANNkd_tree(Pts, sz, 2);
}

void PossionMatte::Matting_Gauss()
{
	std::vector<cv::Point2i>Tu;
	int** id = new int* [H];
	for (int i = 0; i < H; ++i)
	{
		id[i] = new int[W];
		for (int j = 0; j < W; ++j)
		{
			if (Trimap[i][j] == 2) {
				id[i][j] = Tu.size();
				Tu.push_back(cv::Point2i(i, j));
			}
		}
	}
	ANNidx index[1];
	ANNdist dist[1];
	ANNpoint pt = annAllocPt(2);
	ANNpointArray Pts[2];
	alpha = new double* [H];
	for (int i = 0; i < H; ++i)
		alpha[i] = new double[W];

	//initialize alpha
	for (int i = 0; i < H; ++i)
		for (int j = 0; j < W; ++j)
			alpha[i][j] = Trimap[i][j] == 2 ? 0.5 : Trimap[i][j];
	for (int iter = 0; iter < 10; ++iter)
	{
		Tu.clear();
		for (int i = 0; i < H; ++i)
			for (int j = 0; j < W; ++j)
				if (Trimap[i][j] == 2) {
					id[i][j] = Tu.size();
					Tu.push_back(cv::Point2i(i, j));
				}
		double* Sol = new double[Tu.size()];
		double* nalpha = new double[Tu.size()];
		tree_bk = Get_Boundary(0, bd_background, Pts[0]); //update FB
		tree_fg = Get_Boundary(1, bd_foreground, Pts[1]);
		cv::Mat_<uchar>Fore = cv::Mat_<uchar>::zeros(H, W);
		cv::Mat_<uchar>Back = cv::Mat_<uchar>::zeros(H, W);
		cv::Mat_<double>FB = cv::Mat_<double>::zeros(H, W);
		for (int i = 0; i < H; ++i)
			for (int j = 0; j < W; ++j)
			{
				if (Trimap[i][j] == 0) 
					FB(i, j) = -I[i][j];
				else if (Trimap[i][j] == 1)
					FB(i, j) = I[i][j];
				else
				{
					pt[0] = i; pt[1] = j;
					tree_fg->annkSearch(pt, 1, index, dist);
					Fore(i, j) = I[bd_foreground[index[0]].x][bd_foreground[index[0]].y];
					tree_bk->annkSearch(pt, 1, index, dist);
					Back(i, j) = I[bd_background[index[0]].x][bd_background[index[0]].y];
					FB(i, j) = Fore(i, j) - Back(i, j);
				}
				//if (FB(i, j) == 0)
				//	FB(i, j) = 1e-9;
			}
		cv::GaussianBlur(FB, FB, cv::Size(9, 9), 0);

		for (int i = 0; i < Tu.size(); ++i) //acaulate div(I/(F-B)) into Sol
		{
			int x = Tu[i].x, y = Tu[i].y;
			Sol[i] = 0;
			double lap_I = 0;
			for (int k = 0; k < 4; ++k)
			{
				int ni = x + dx[k], nj = y + dy[k];
				if (ni < 0 || ni >= H || nj < 0 || nj >= W)
					continue;
				lap_I += I[ni][nj] - I[x][y];
			}
			if (fabs(FB(x, y)) > 1e-9)
			{
				//std::cout << x << " " << y <<" "<< FB(x, y) << std::endl;
				double FBI_dx = (x != H - 1) ? (FB(x + 1, y) - FB(x, y)) * (I[x + 1][y] - I[x][y]) : 0;
				double FBI_dy = (y != W - 1) ? (FB(x, y + 1) - FB(x, y)) * (I[x][y + 1] - I[x][y]) : 0;
				//std::cout << (lap_I * FB(x, y) - FBI_dx - FBI_dy) / FB(x, y) / FB(x, y) << std::endl;
				Sol[i] += (lap_I * FB(x, y) - FBI_dx - FBI_dy) / FB(x, y) / FB(x, y);
			}
		}

		for (int iter = 0; iter < 300; ++iter)
		{
			for (int i = 0; i < Tu.size(); ++i)
			{
				int x = Tu[i].x, y = Tu[i].y; int d = 0;
				nalpha[i] = 0;
				for (int k = 0; k < 4; ++k)
				{
					int ni = x + dx[k], nj = y + dy[k];
					if (ni < 0 || ni >= H || nj < 0 || nj >= W)
						continue;
					++d;
					nalpha[i] += alpha[ni][nj];
				}
				nalpha[i] -= Sol[i];
				nalpha[i] /= d;
			}

			for (int i = 0; i < Tu.size(); ++i)
				alpha[Tu[i].x][Tu[i].y] = nalpha[i] <= 0 ? 0 : nalpha[i] >= 1 ? 1 : nalpha[i];
		}
		bool isnew = false;
		int therod = 2;
		for (int i = 0; i < Tu.size(); ++i) {
			double _alpha = alpha[Tu[i].x][Tu[i].y];
			if (_alpha <= 0.05 && abs(I[Tu[i].x][Tu[i].y] - Back[Tu[i].x][Tu[i].y]) <= therod)
			{
				Trimap[Tu[i].x][Tu[i].y] = 0;
				isnew = true;
			}
			if(_alpha >= 0.95 && abs(I[Tu[i].x][Tu[i].y] - Fore[Tu[i].x][Tu[i].y]) <= therod)
			{
				Trimap[Tu[i].x][Tu[i].y] = 1;
				isnew = true;
			}
		}
		printf("%d\n", iter);
		if (!isnew)
			break;
	}
	cv::Mat_<uchar>alpha_display = cv::Mat_<uchar>::zeros(H, W);
	for (int i = 0; i < H; ++i)
		for (int j = 0; j < W; ++j)
			alpha_display(i, j) = alpha[i][j] * 255;
	cv::imshow("display", alpha_display);
	for (int i = 0; i < H; ++i)
		delete[] id[i];
	delete[] id;
}

void PossionMatte::Matting_Eigen() //Old Version
{
	std::vector<cv::Point2i>Tu;
	int **id = new int* [H];
	for (int i = 0; i < H; ++i)
	{
		id[i] = new int[W];
		for (int j = 0; j < W; ++j)
		{
			if (Trimap[i][j] == 2) {
				id[i][j] = Tu.size();
				Tu.push_back(cv::Point2i(i, j));
			}
		}
	}
	VectorXd B; SpMat A;
	ANNidx index[1];
	ANNdist dist[1];
	ANNpoint pt = annAllocPt(2);
	ANNpointArray Pts[2];
	for (int iter = 0; iter < 5; ++iter)
	{
		Tu.clear();
		for (int i = 0; i < H; ++i)
			for (int j = 0; j < W; ++j)
				if (Trimap[i][j] == 2) {
					id[i][j] = Tu.size();
					Tu.push_back(cv::Point2i(i, j));
				}
		std::vector<T>coeff;
		for (auto p : Tu)
		{
			int d = 0, p_id = id[p.x][p.y];
			for (int k = 0; k < 4; ++k)
			{
				int ni = p.x + dx[k], nj = p.y + dy[k];
				if (ni < 0 || ni >= H || nj < 0 || nj >= W)
					continue;
				if (Trimap[ni][nj] == 2)
					coeff.push_back(T(p_id, id[ni][nj], 1));
				++d;
			}
			assert(d != 0);
			coeff.push_back(T(p_id, p_id, -d));
		}
		A.resize(Tu.size(), Tu.size());
		A.setFromTriplets(coeff.begin(), coeff.end());
		SimplicialCholesky<SpMat>solver;
		solver.compute(A);
		assert(solver.info() == Success);
		B.resize(Tu.size());
		tree_bk = Get_Boundary(0, bd_background, Pts[0]); //update FB
		tree_fg = Get_Boundary(1, bd_foreground, Pts[1]);
		cv::Mat_<uchar>Fore = cv::Mat_<uchar>::zeros(H, W);
		cv::Mat_<uchar>Back = cv::Mat_<uchar>::zeros(H, W);
		cv::Mat_<double>FB = cv::Mat_<double>::zeros(H, W);
		for (int i = 0; i < H; ++i)
			for (int j = 0; j < W; ++j)
			{
				if (Trimap[i][j] == 0)
					FB(i, j) = -I[i][j];
				else if(Trimap[i][j] == 1)
					FB(i, j) = I[i][j];
				else
				{
					pt[0] = i; pt[1] = j;
					tree_fg->annkSearch(pt, 1, index, dist);
					Fore(i, j) = I[bd_foreground[index[0]].x][bd_foreground[index[0]].y];
					tree_bk->annkSearch(pt, 1, index, dist);
					Back(i, j) = I[bd_background[index[0]].x][bd_background[index[0]].y];
					FB(i, j) = Fore(i, j) - Back(i, j);
				}
				//if (FB(i, j) == 0)
				//	FB(i, j) = 0.1;
			}
		cv::GaussianBlur(FB, FB, cv::Size(9, 9), 0);

		for (int i = 0; i < Tu.size(); ++i)
		{
			int x = Tu[i].x, y = Tu[i].y;
			B[i] = 0;
			double lap_I = 0;
			for (int k = 0; k < 4; ++k)
			{
				int ni = x + dx[k], nj = y + dy[k];
				if (ni < 0 || ni >= H || nj < 0 || nj >= W)
					continue;
				lap_I += I[ni][nj] - I[x][y];
				if (Trimap[ni][nj] < 2)
					B[i] += -Trimap[ni][nj];
			}
			if (fabs(FB(x, y)) > 1e-9)
			{
				//std::cout << x << " " << y <<" "<< FB(x, y) << std::endl;
				double FBI_dx = (x != H - 1) ? (FB(x + 1, y) - FB(x, y)) * (I[x + 1][y] - I[x][y]) : 0;
				double FBI_dy = (y != W - 1) ? (FB(x, y + 1) - FB(x, y)) * (I[x][y + 1] - I[x][y]) : 0;
				//std::cout << (lap_I * FB(x, y) - FBI_dx - FBI_dy) / FB(x, y) / FB(x, y) << std::endl;
				B[i] += (lap_I * FB(x, y) - FBI_dx - FBI_dy) / FB(x, y) / FB(x, y);
			}
		}
		B = solver.solve(B);
		//std::cout << B.size() << std::endl;
		//system("pause");
		bool isnew = false;
		int therod = 2;
		for (int i = 0; i < Tu.size(); ++i) {
			double _alpha = B(i);
			if (_alpha <= 0.05 && abs(I[Tu[i].x][Tu[i].y] - Back[Tu[i].x][Tu[i].y]) <= therod)
			{
				Trimap[Tu[i].x][Tu[i].y] = 0;
				isnew = true;
			}
			if (_alpha >= 0.95 && abs(I[Tu[i].x][Tu[i].y] - Fore[Tu[i].x][Tu[i].y]) <= therod)
			{
				Trimap[Tu[i].x][Tu[i].y] = 1;
				isnew = true;
			}
		}
		if (!isnew)
			break;
	}
	alpha = new double* [H];
	for (int i = 0; i < H; ++i)
	{
		alpha[i] = new double[W];
		for (int j = 0; j < W; ++j)
			alpha[i][j] = Trimap[i][j];
	}
	for (int i = 0; i < Tu.size(); ++i)
	{
		int px = Tu[i].x, py = Tu[i].y;
		if (Trimap[px][py] == 2)
			alpha[px][py] = B(i);
	}
	for (int i = 0; i < H; ++i)
		delete[] id[i];
	delete[] id;
}
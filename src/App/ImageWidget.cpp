#include "ImageWidget.h"
#include <QImage>
#include <QPainter>
#include <QtWidgets> 
#include <iostream>
#include "PossionMatte.h"

using std::cout;
using std::endl;

ImageWidget::ImageWidget(void)
{
	draw_status = false;
	ChooseStatus = kNone;
	CurrentPolygon = ForePolygon = BackPolygon = NULL;
}


ImageWidget::~ImageWidget(void)
{
}

void ImageWidget::paintEvent(QPaintEvent *paintevent)
{
	QPainter painter;
	painter.begin(this);

	// Draw background
	painter.setBrush(Qt::lightGray);

	QRect back_rect(0, 0, width(), height());
	painter.drawRect(back_rect);

	// Draw image
	QImage image_show = QImage((unsigned char*)(image_mat_.data), image_mat_.cols, image_mat_.rows, image_mat_.step, QImage::Format_RGB888);
	image_start_position.setX((width() - image_show.width()) / 2);
	image_start_position.setY((height() - image_show.height()) / 2);
	QRect rect = QRect( (width()- image_show.width())/2, (height()- image_show.height())/2, image_show.width(), image_show.height());
	painter.drawImage(rect, image_show);


	// Draw choose region
	painter.setBrush(Qt::NoBrush);
	if (ForePolygon != NULL)
	{
		painter.setPen(Qt::red);
		ForePolygon->Draw(painter);
	}
	if (BackPolygon != NULL)
	{
		painter.setPen(Qt::blue);
		BackPolygon->Draw(painter);
	}

	painter.end();
}


void ImageWidget::mousePressEvent(QMouseEvent* event)
{
	if (Qt::LeftButton == event->button() && ChooseStatus != kNone)
	{
		if (draw_status) 
		{
			assert(CurrentPolygon != NULL);
			CurrentPolygon->addPoint();
		}
		else {
			CurrentPolygon = new CPolygon(event->pos());
			if (ChooseStatus == kBackPolygon) 
				BackPolygon = CurrentPolygon;
			else ForePolygon = CurrentPolygon;
			setMouseTracking(true);
			draw_status = true;
		}
	}
	update();
}

void ImageWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (draw_status && ChooseStatus != kNone)
	{
		assert(CurrentPolygon != NULL);
		CurrentPolygon->set_end(event->pos());
	}
	update();
}

void ImageWidget::mouseReleaseEvent(QMouseEvent* event)
{
	update();
}


void ImageWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (ChooseStatus != kNone)
	{
		CurrentPolygon->addPoint();
		ChooseStatus = kNone;
		draw_status = false;
		setMouseTracking(false);
	}
	update();
}

void ImageWidget::Open()
{
	// Open file
	QString fileName = QFileDialog::getOpenFileName(this, tr("Read Image"), ".", tr("Images(*.bmp *.png *.jpg)"));

	// Load file
	if (!fileName.isEmpty())
	{
		image_mat_ = cv::imread(fileName.toLatin1().data());
		cv::cvtColor(image_mat_, image_mat_, cv::COLOR_BGR2RGB);
		image_mat_backup_ = image_mat_.clone();
	}

	update();
}

void ImageWidget::Save()
{
	SaveAs();
}

void ImageWidget::SaveAs()
{
	QString filename = QFileDialog::getSaveFileName(this, tr("Save Image"), ".", tr("Images(*.bmp *.png *.jpg)"));
	if (filename.isNull())
	{
		return;
	}

	cv::Mat image_save;
	cv::cvtColor(image_mat_, image_save, cv::COLOR_BGR2RGB);
	cv::imwrite(filename.toLatin1().data(), image_save);
}

void ImageWidget::Invert()
{
	if (image_mat_.empty())
		return;
	cv::MatIterator_<cv::Vec3b> iter, iterend;
	for (iter = image_mat_.begin<cv::Vec3b>(), iterend = image_mat_.end<cv::Vec3b>(); iter != iterend; ++iter)
	{
		(*iter)[0] = 255 - (*iter)[0];
		(*iter)[1] = 255 - (*iter)[1];
		(*iter)[2] = 255 - (*iter)[2];
	}

	update();
}

void ImageWidget::Mirror(bool ishorizontal, bool isvertical)
{
	if (image_mat_.empty())
		return;
	int width = image_mat_.cols;
	int height = image_mat_.rows;

	if (ishorizontal)
	{
		if (isvertical)
		{
			for (int i = 0; i < width; i++)
			{
				for (int j = 0; j < height; j++)
				{
					image_mat_.at<cv::Vec3b>(j, i) = image_mat_backup_.at<cv::Vec3b>(height - 1 - j, width - 1 - i);
				}
			}
		}
		else
		{
			for (int i = 0; i < width; i++)
			{
				for (int j = 0; j < height; j++)
				{
					image_mat_.at<cv::Vec3b>(j, i) = image_mat_backup_.at<cv::Vec3b>(j, width - 1 - i);
				}
			}
		}

	}
	else
	{
		if (isvertical)
		{
			for (int i = 0; i < width; i++)
			{
				for (int j = 0; j < height; j++)
				{
					image_mat_.at<cv::Vec3b>(j, i) = image_mat_backup_.at<cv::Vec3b>(height - 1 - j, i);
				}
			}
		}
	}

	update();
}

void ImageWidget::TurnGray()
{
	if (image_mat_.empty())
		return;
	cv::MatIterator_<cv::Vec3b> iter, iterend;
	for (iter = image_mat_.begin<cv::Vec3b>(), iterend = image_mat_.end<cv::Vec3b>(); iter != iterend; ++iter)
	{
		int itmp = ((*iter)[0] + (*iter)[1] + (*iter)[2]) / 3;
		(*iter)[0] = itmp;
		(*iter)[1] = itmp;
		(*iter)[2] = itmp;
	}

	update();
}

void ImageWidget::Restore()
{
	image_mat_ = image_mat_backup_.clone();
	update();
}

void ImageWidget::ChooseForeground()
{
	ChooseStatus = kForePolygon;
}

void ImageWidget::ChooseBackground()
{
	ChooseStatus = kBackPolygon;
}


void ImageWidget::Cut()
{
	int H = image_mat_.rows;
	int W = image_mat_.cols;
	int** Trimap = new int*[H];
	for (int i = 0; i < H; ++i)
		Trimap[i] = new int[W]{0};
	auto Region = BackPolygon->Get_Inside();
	for (auto p : Region) 
	{
		p -= image_start_position;
		int x = p.y(), y = p.x();
		if (x < 0 || x >= H || y < 0 || y >= W)
			continue;
		//std::cout << x << " " << y << std::endl;
		Trimap[x][y] = 2;
	}

	Region = ForePolygon->Get_Inside();
	for (auto p : Region)
	{
		p -= image_start_position;
		int x = p.y(), y = p.x();
		if (x < 0 || x >= H || y < 0 || y >= W)
			continue;
		Trimap[x][y] = 1;
	}
	int **I = new int* [H];
	cv::MatIterator_<cv::Vec3b> iter = image_mat_.begin<cv::Vec3b>();
	for (int i = 0; i < H; ++i)
	{
		I[i] = new int[W];
		for (int j = 0; j < W; ++j)
		{
			I[i][j] = ((*iter)[0] + (*iter)[1] + (*iter)[2]) / 3;
			++iter;
		}
	}
	PossionMatte Matting_Tool(image_mat_.rows, image_mat_.cols, I, Trimap);
	Matting_Tool.Matting_Gauss();
	for (int i = 0; i < H; ++i)
		for (int j = 0; j < W; ++j)
			image_mat_.at<cv::Vec3b>(i, j) *= Matting_Tool.alpha[i][j];
	update();
}
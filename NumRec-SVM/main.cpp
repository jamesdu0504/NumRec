#include <iostream>
#include <fstream>
#include "opencv2/opencv.hpp"
#include <vector>

using namespace std;
using namespace cv;

#define SHOW_PROCESS 0
#define ON_STUDY 1

class NumTrainData
{
public:
	NumTrainData()
	{
		memset(data, 0, sizeof(data));
		result = -1;
	}
public:
	float data[64];
	int result;
};

vector<NumTrainData> buffer;
int featureLen = 64;

//测试样本数据中数值为大端的，swapBuffer通过交换将大端转为小端数值
void swapBuffer(char* buf)
{
	char temp;
	temp = *(buf);
	*buf = *(buf+3);
	*(buf+3) = temp;

	temp = *(buf+1);
	*(buf+1) = *(buf+2);
	*(buf+2) = temp;
}
//将目标周围背景去除
void GetROI(Mat& src, Mat& dst)
{
	int left, right, top, bottom;
	left = src.cols;
	right = 0;
	top = src.rows;
	bottom = 0;

	//Get valid area
	for(int i=0; i<src.rows; i++)
	{
		for(int j=0; j<src.cols; j++)
		{
			if(src.at<uchar>(i, j) > 0)//排除黑点（背景），寻找目标点；
			{
				if(j<left) left = j;
				if(j>right) right = j;
				if(i<top) top = i;
				if(i>bottom) bottom = i;
			}
		}
	}

	//Point center;
	//center.x = (left + right) / 2;
	//center.y = (top + bottom) / 2;

	int width = right - left;
	int height = bottom - top;
	int len = (width < height) ? height : width;

	//Create a square
	dst = Mat::zeros(len, len, CV_8UC1);

	//Copy valid data to square center
	Rect dstRect((len - width)/2, (len - height)/2, width, height);
	Rect srcRect(left, top, width, height);
	Mat dstROI = dst(dstRect);
	Mat srcROI = src(srcRect);
	srcROI.copyTo(dstROI);
}

int ReadTrainData(int maxCount)
{
	//Open image and label file
	const char fileName[] = "train-images.idx3-ubyte";
	const char labelFileName[] = "train-labels.idx1-ubyte";
	ifstream lab_ifs(labelFileName, ios_base::binary);//ifstream读取文件，此处为二进制文件
	ifstream ifs(fileName, ios_base::binary);

	if(( lab_ifs.fail() == true )||( ifs.fail() == true ))
		return -1;

	//Read train data number and image rows / cols
	char magicNum[4], ccount[4], crows[4], ccols[4];
	//Just skip image header
	ifs.read(magicNum, sizeof(magicNum));
	ifs.read(ccount, sizeof(ccount));

	ifs.read(crows, sizeof(crows));
	ifs.read(ccols, sizeof(ccols));

	int count, rows, cols;
	swapBuffer(ccount);
	swapBuffer(crows);
	swapBuffer(ccols);

	memcpy(&count, ccount, sizeof(count));//原型void *memcpy(void *dest, const void *src, size_t n);
	memcpy(&rows, crows, sizeof(rows)); // 从源src所指的内存地址的起始位置开始拷贝n个字节到目标dest所指的内存地址的起始位置中
	memcpy(&cols, ccols, sizeof(cols));

	//Just skip label header
	lab_ifs.read(magicNum, sizeof(magicNum));
	lab_ifs.read(ccount, sizeof(ccount));

	//Create source and show image matrix
	Mat src = Mat::zeros(rows, cols, CV_8UC1);
	Mat temp = Mat::zeros(8, 8, CV_8UC1);//8*8 feature extraction
	Mat img, dst;

	char label = 0;
	Scalar templateColor(255, 0, 255 );

	NumTrainData rtd;

	//int loop = 1000;
	int total = 0;

	while(!ifs.eof())
	{
		if(total >= count)
			break;

		total++;
		cout << total << endl;

		//Read label
		lab_ifs.read(&label, 1);
		label = label + '0';

		//Read source data
		ifs.read((char*)src.data, rows * cols);
		GetROI(src, dst);

#if(SHOW_PROCESS)
		//Too small to watch
		img = Mat::zeros(dst.rows*10, dst.cols*10, CV_8UC1);
		resize(dst, img, img.size());

		stringstream ss;
		ss << "Number " << label;
		string text = ss.str();
		putText(img, text, Point(10, 50), FONT_HERSHEY_SIMPLEX, 1.0, templateColor);

		//imshow("img", img);
#endif

		rtd.result = label;
		resize(dst, temp, temp.size());
		//threshold(temp, temp, 10, 1, CV_THRESH_BINARY);

		for(int i = 0; i<8; i++)
		{
			for(int j = 0; j<8; j++)
			{
					rtd.data[ i*8 + j] = temp.at<uchar>(i, j);
			}
		}

		buffer.push_back(rtd);

		//if(waitKey(0)==27) //ESC to quit
		//	break;

		maxCount--;

		if(maxCount == 0)
			break;
	}

	ifs.close();
	lab_ifs.close();

	return 0;
}


void newSvmStudy(vector<NumTrainData>& trainData)
{
	int testCount = trainData.size();

	Mat m = Mat::zeros(1, featureLen, CV_32FC1);// feature matrix(1*featureLen) of one image
	Mat data = Mat::zeros(testCount, featureLen, CV_32FC1);// feature matrix(testcount*featureLen) of 'testcount' images
	Mat res = Mat::zeros(testCount, 1, CV_32SC1);// labels matrix(testcount*1) of 'testcount' images

	for (int i= 0; i< testCount; i++)
	{

		NumTrainData td = trainData.at(i);
		memcpy(m.data, td.data, featureLen*sizeof(float));
		normalize(m, m);
		memcpy(data.data + i*featureLen*sizeof(float), m.data, featureLen*sizeof(float));

		res.at<unsigned int>(i, 0) = td.result;
	}

	/////////////START SVM TRAINNING//////////////////

	svm.train(data, res, Mat(), Mat(), param);
	svm.save( "SVM_DATA.xml" );
}


int newSvmPredict()
{
	CvSVM svm = CvSVM();
	svm.load( "SVM_DATA.xml" );

	const char fileName[] = "t10k-images.idx3-ubyte";
	const char labelFileName[] = "t10k-labels.idx1-ubyte";

	ifstream lab_ifs(labelFileName, ios_base::binary);
	ifstream ifs(fileName, ios_base::binary);

	if( ifs.fail() == true )
		return -1;
	if( lab_ifs.fail() == true )
		return -1;

	char magicNum[4], ccount[4], crows[4], ccols[4];
	ifs.read(magicNum, sizeof(magicNum));
	ifs.read(ccount, sizeof(ccount));
	ifs.read(crows, sizeof(crows));
	ifs.read(ccols, sizeof(ccols));

	int count, rows, cols;
	swapBuffer(ccount);
	swapBuffer(crows);
	swapBuffer(ccols);

	memcpy(&count, ccount, sizeof(count));
	memcpy(&rows, crows, sizeof(rows));
	memcpy(&cols, ccols, sizeof(cols));

	Mat src = Mat::zeros(rows, cols, CV_8UC1);
	Mat temp = Mat::zeros(8, 8, CV_8UC1);
	Mat m = Mat::zeros(1, featureLen, CV_32FC1);
	Mat img, dst;

	//Just skip label header
	lab_ifs.read(magicNum, sizeof(magicNum));
	lab_ifs.read(ccount, sizeof(ccount));

	char label = 0;
	Scalar templateColor(255, 0, 0);

	NumTrainData rtd;

	int right = 0, error = 0, total = 0;
	int right_1 = 0, error_1 = 0, right_2 = 0, error_2 = 0;
	while(ifs.good())
	{
		//Read label
		lab_ifs.read(&label, 1);
		label = label + '0';

		//Read data
		ifs.read((char*)src.data, rows * cols);
		GetROI(src, dst);

		//Too small to watch
		img = Mat::zeros(dst.rows*30, dst.cols*30, CV_8UC3);
		resize(dst, img, img.size());

		rtd.result = label;
		resize(dst, temp, temp.size());
		//threshold(temp, temp, 10, 1, CV_THRESH_BINARY);
		for(int i = 0; i<8; i++)
		{
			for(int j = 0; j<8; j++)
			{
					m.at<float>(0,j + i*8) = temp.at<uchar>(i, j);
			}
		}

		if(total >= count)
			break;

		normalize(m, m);
		char ret = (char)svm.predict(m);

		if(ret == label)
		{
			right++;
			if(total <= 5000)
				right_1++;
			else
				right_2++;
		}
		else
		{
			error++;
			if(total <= 5000)
				error_1++;
			else
				error_2++;
		}

		total++;

#if(SHOW_PROCESS)
		stringstream ss;
		ss << "Number " << label << ", predict " << ret;
		string text = ss.str();
		putText(img, text, Point(10, 50), FONT_HERSHEY_SIMPLEX, 1.0, templateColor);

		imshow("img", img);
		if(waitKey(0)==27) //ESC to quit
			break;
#endif

	}

	ifs.close();
	lab_ifs.close();

	stringstream ss;
	ss << "Total " << total << ", right " << right <<", error " << error;
	cout << "Accuracy= " << 100*right/float(total)<<"%";

	string text = ss.str();
	putText(img, text, Point(50, 50), FONT_HERSHEY_SIMPLEX, 1.0, templateColor);
	imshow("img", img);
	waitKey(0);

	return 0;
}

int main( int argc, char *argv[] )
{
#if(ON_STUDY)
	int maxCount = 1000;
	ReadTrainData(maxCount);

	//newRtStudy(buffer);
	newSvmStudy(buffer);
#else
	//newRtPredict();
	newSvmPredict();
#endif
	return 0;
}

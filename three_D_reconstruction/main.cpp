#include <iostream>
#include <sstream>
#include <time.h>
#include <stdio.h>

#include <opencv2/core.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include<opencv/cv.h>

#ifndef _CRT_SECURE_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS
#endif

using namespace cv;
using namespace std;
enum
{
	CAMERA=0,IMAGELIST=1
};
enum
{
	BM=1,SGBM=0
};

bool get_imagePair=false;

float X=0;
float Y=0;
float Z=0;

Size mousePoint=Size(0,0);

Size imageSize;//�궨ͼ��ķֱ��ʣ�960x720��
Size boardSize;
int nrFrames=0;
Mat cameraMatrix1;//������ڲξ���
Mat distCoeffs1;//���������ϵ������
Mat cameraMatrix2;//������ڲξ���
Mat distCoeffs2;//���������ϵ������
Mat R, T, E, F; //R ��תʸ�� Tƽ��ʸ�� E�������� F��������
Mat R1, R2, P1, P2, Q; //У����ת����R��ͶӰ����P ��ͶӰ����Q 
Mat mapx1, mapy1, mapx2, mapy2; //ӳ���  
Rect validROI1, validROI2; //ͼ��У��֮�󣬻��ͼ����вü��������validROI����ָ�ü�֮�������  
bool isopen=true;//�����ж������ȡͼ���ʱ�Ƿ��
Mat imgLeft;//У�������ͼ��
Mat imgRight;//У�������ͼ��
Mat disparity8U;//���Ӳ�ֵ��ΧͶӰ��0-255���Ӳ�ͼ
Mat disparity_real;//��ʵ�Ӳ�ֵ���Ӳ�ͼ
Mat disparity;//���ú����õ���ԭʼ�Ӳ�ͼ
int numDisparities = 7;//ƥ���������ӲΧ���涨�����ܱ�16����
int blockSize = 7;//ƥ�䴰�ڴ�С
int UniquenessRatio=5;
Ptr<StereoBM> bm = StereoBM::create(16,21);
Mat _3dImage;//��ά����ͼ
//�������ص�����on_mouse()����������ָ���㣨������������Ӳ�ֵ����άXYZֵ����λ�����ף�
void on_mouse(int event,int x,int y,int flags,void* a)
{
	if(event==CV_EVENT_LBUTTONDOWN)//�������
	{
		mousePoint.width=x;
		mousePoint.height=y;
		//cout<<"x="<<mousePoint.width<<'\t'<<"y="<<mousePoint.height<<endl;
	}
}
//�������ص�����on_mouse1()���ڻ�ȡ����õ�����ͼ��ԣ�˫�������
void on_mouse1(int event,int x,int y,int flags,void* a)
{
	if(event==CV_EVENT_LBUTTONDBLCLK)//˫�����
	{
		get_imagePair=true;
	}
}
bool readFile(string filename)
{
	FileStorage fs(filename,FileStorage::READ);
	if(fs.isOpened())
	{
		fs["width"]>>imageSize.width;
		fs["height"]>>imageSize.height;
		fs["board_width"]>> boardSize.width;
		fs["board_height"]>> boardSize.height;
		fs["nrFrames"]>>nrFrames;
		fs["cameraMatrix1"]>>cameraMatrix1;
		fs["distCoeffs1"]>>distCoeffs1;
		fs["cameraMatrix2"]>>cameraMatrix2;
		fs["distCoeffs2"]>>distCoeffs2;
		fs["R"] >> R;      
        fs["T"] >> T;
		fs["E"] >> E;      
        fs["F"] >> F;
		fs.release();
		/*
		cout<<"Succeed to read the Calibration result!!!"<<endl;
		cout<<"������ڲξ���"<<endl<<cameraMatrix1<<endl;
		cout<<"������ڲξ���"<<endl<<cameraMatrix2<<endl;
		cout<<"R:"<<endl<<R<<endl;
		cout<<"T:"<<endl<<T<<endl;
		cout<<"E:"<<endl<<E<<endl;
		cout<<"F:"<<endl<<F<<endl;
		*/
		return true;
	}
	else
	{  
        cerr<< "Error: can not open the Calibration result file!!!!!" << endl;
		return false;
    }
}
void stereo_rectify(bool useCalibrated,int getImagePair,string path1,string path2)
{
	/**************************************************************************************/
	/*
	Ҫʵ������У����ʹ����ͼ��ƽ�����ж�׼����Ҫ�õ����²�����
	cameraMatrix1, distCoeffs1, R1, P1
	cameraMatrix2, distCoeffs2, R2, P2
	�����ڲξ���ͻ���ϵ��ͨ���궨�����ã���R1��P1��R2��P2��ֵ��opencv�ṩ�����ַ�����
	1. Hartley������
	���ַ�����Ϊ���Ǳ궨����У����������Ҳ����˵����ͨ���궨��õ��ڲξ���ͻ���ϵ����ȡ
	R1��P1��R2��P2��ֵ��ֱ�Ӹ���ƥ�������������F���ٽ�һ������R1��P1��R2��P2��
	���ַ�����Ҫ�õ�����������findFundamentalMat()��stereoRectifyUncalibrated()
	�����ԭ��˵���ο���Learning opencv�����İ�498ҳ��
	2. Bouguet���� 
	���ַ�����Ϊ���궨����У�������������Ǹ�������궨��õ��ڲξ��󡢻���ϵ����R��T��Ϊ
	���룬����stereoRectify()�����õ�R1��P1��R2��P2��ֵ��
	���ַ�����ѡ�ø���bool���͵�useCalibrated����������
	��useCalibrated=trueʱ������Bouguet������
	��useCalibrated=falseʱ������Hartley������
	*/
	/**************************************************************************************/
	//��useCalibrated=trueʱ������Bouguet����
    if( useCalibrated )
    {
        //����alpha�����öԽ��Ӱ��ܴ�
		//alpha��ͼ�����ϵ����ȡֵ��Χ��-1��0~1��
		//��ȡֵΪ 0 ʱ��OpenCV���У�����ͼ��������ź�ƽ�ƣ�ʹ��remapͼ��ֻ��ʾ��Ч���أ���ȥ��������ı߽�����,�����ڻ����˱��ϵ�����Ӧ�ã�
		//��alphaȡֵΪ1ʱ��remapͼ����ʾ����ԭͼ���а��������أ���ȡֵ�����ڻ���ϵ�����ٵĸ߶�����ͷ��
		//alphaȡֵ��0-1֮��ʱ��OpenCV����Ӧ��������ԭͼ��ı߽��������ء�
		//AlphaȡֵΪ-1ʱ��OpenCV�Զ��������ź�ƽ��
		stereoRectify(cameraMatrix1, distCoeffs1, cameraMatrix2, distCoeffs2, imageSize, R, T, R1, R2, P1, P2, Q,  
                  CALIB_ZERO_DISPARITY,0,imageSize,&validROI1,&validROI2);
    }
	//��useCalibrated=falseʱ������Hartley����
    else
    {
		vector<vector<Point2f> > imagePoints1;
		vector<vector<Point2f> > imagePoints2;
		char name[100];
		for(int i=1;i<=nrFrames;i++)
		{
			//sprintf(name,"Calibration_Image_Camera\\Image%d%s",i,".jpg");
			//imageList.push_back(name);
			sprintf(name,"Calibration_Image_Camera\\Image_l%d%s",i,".jpg");
			Mat src1=imread(name,1);
			vector<Point2f> pointBuf1;
			bool found1=findChessboardCorners( src1, boardSize, pointBuf1);
			if(found1)
			{
				Mat grayimage1;
				cvtColor(src1, grayimage1, COLOR_BGR2GRAY);
				cornerSubPix( grayimage1, pointBuf1, Size(11,11),Size(-1,-1), TermCriteria( TermCriteria::EPS+TermCriteria::COUNT, 30, 0.1 ));
				imagePoints1.push_back(pointBuf1);
			}
			sprintf(name,"Calibration_Image_Camera\\Image_r%d%s",i,".jpg");
			Mat src2=imread(name,1);
			vector<Point2f> pointBuf2;
			bool found2=findChessboardCorners( src2, boardSize, pointBuf2);
			if(found2)
			{
				Mat grayimage2;
				cvtColor(src2, grayimage2, COLOR_BGR2GRAY);
				cornerSubPix( grayimage2, pointBuf2, Size(11,11),Size(-1,-1), TermCriteria( TermCriteria::EPS+TermCriteria::COUNT, 30, 0.1 ));
				imagePoints2.push_back(pointBuf2);
			}
		}
        vector<Point2f> allimgpt[2];
        for(int i = 0; i < nrFrames; i++ )
			copy(imagePoints1[i].begin(), imagePoints1[i].end(), back_inserter(allimgpt[0]));
		for(int i = 0; i < nrFrames; i++ )
			copy(imagePoints2[i].begin(), imagePoints2[i].end(), back_inserter(allimgpt[1]));
        F = findFundamentalMat(Mat(allimgpt[0]), Mat(allimgpt[1]), FM_8POINT, 0, 0);
        Mat H1, H2;
        stereoRectifyUncalibrated(Mat(allimgpt[0]), Mat(allimgpt[1]), F, imageSize, H1, H2, 3);

        R1 = cameraMatrix1.inv()*H1*cameraMatrix1;
        R2 = cameraMatrix2.inv()*H2*cameraMatrix2;
        P1 = cameraMatrix1;
        P2 = cameraMatrix2;
    }
	/* 
    ����stereoRectify���������R��P������ͼ���ӳ���mapx,mapy 
    mapx,mapy������ӳ�����������Ը�remap()�������ã���У��ͼ��ʹ������ͼ���沢���ж�׼ 
    initUndistortRectifyMap()�Ĳ���newCameraMatrix����У��������������
	��openCV���棬У����ļ��������Mrect�Ǹ�ͶӰ����Pһ�𷵻صġ� 
    �������������ﴫ��ͶӰ����P���˺������Դ�ͶӰ����P�ж���У�������������� 
    */
	initUndistortRectifyMap(cameraMatrix1, distCoeffs1, R1, P1, imageSize, CV_16SC2, mapx1, mapy1);  
    initUndistortRectifyMap(cameraMatrix2, distCoeffs2, R2, P2, imageSize, CV_16SC2, mapx2, mapy2); 
	if(getImagePair==CAMERA)
	{
		VideoCapture inputCapture1;
		VideoCapture inputCapture2;
		inputCapture2.open(2);
		inputCapture1.open(1);
		if(!inputCapture1.isOpened()==true) 
		{
			cerr<<"failed to open camera1"<<endl;
			isopen=false;
			return;
		}
		if(!inputCapture2.isOpened()==true)
		{
			cerr<<"failed to open camera2"<<endl;
			isopen=false;
			return;
		}
		
		inputCapture1.set(CV_CAP_PROP_FRAME_WIDTH, 960);  
		inputCapture1.set(CV_CAP_PROP_FRAME_HEIGHT, 720);
		inputCapture2.set(CV_CAP_PROP_FRAME_WIDTH, 960);  
		inputCapture2.set(CV_CAP_PROP_FRAME_HEIGHT, 720);
		/*
		inputCapture1.set(CV_CAP_PROP_FRAME_WIDTH, 640);  
		inputCapture1.set(CV_CAP_PROP_FRAME_HEIGHT, 480);
		inputCapture2.set(CV_CAP_PROP_FRAME_WIDTH, 640);  
		inputCapture2.set(CV_CAP_PROP_FRAME_HEIGHT, 480);
		*/
		namedWindow("��ȡͼ��Դ��ڣ�˫�������",WINDOW_AUTOSIZE);
		//�������ص�����on_mouse1()���ڻ�ȡ��ά�ؽ��õ�����ͼ���
		//��ָ������˫��������һ�βɼ�һ��ͼ��
		cvSetMouseCallback("��ȡͼ��Դ��ڣ�˫�������",on_mouse1,NULL);
		Mat src_image1;
		Mat src_image2;
		while(1)
		{
			inputCapture1>>src_image1;
			inputCapture2>>src_image2;
			imshow("��ȡͼ��Դ��ڣ�˫�������",src_image1);
			imshow("��ȡͼ��Դ���(��ͼ��)",src_image2);
			waitKey(35);
			if(get_imagePair==true)
			{
				Mat cap1;
				Mat cap2;
				inputCapture1>>cap1;
				inputCapture2>>cap2;
				char address[100];
				sprintf(address,"Stereo_Sample\\ImageL1%s",".jpg");
				imwrite(address,cap1);
				sprintf(address,"Stereo_Sample\\ImageR1%s",".jpg");
				imwrite(address,cap2);
				get_imagePair=false;
				cout<<"ͼ��Բɼ���ϣ�������"<<endl;
				break;
			}
		}
		destroyWindow("��ȡͼ��Դ��ڣ�˫�������");
		destroyWindow("��ȡͼ��Դ��ڣ���ͼ��");
	}
	Mat grayimgLeft = imread(path1, IMREAD_GRAYSCALE );
    Mat grayimgRight = imread(path2, IMREAD_GRAYSCALE );
	Mat medianBlurLeft,medianBlurRight;
	medianBlur(grayimgLeft,medianBlurLeft,5);
	medianBlur(grayimgRight,medianBlurRight,5);
	Mat gaussianBlurLeft,gaussianBlurRight;
	GaussianBlur(medianBlurLeft,gaussianBlurLeft,Size(5,5),0,0);
	GaussianBlur(medianBlurRight,gaussianBlurRight,Size(5,5),0,0);

	//imshow("ImageL",grayimgLeft);
    //imshow("ImageR",grayimgRight);
    
    //����remap֮�����������ͼ���Ѿ����沢���ж�׼�� 
	//remap(grayimgLeft, imgLeft, mapx1, mapy1, INTER_LINEAR);  
    //remap(grayimgRight, imgRight, mapx2, mapy2, INTER_LINEAR);
	remap(gaussianBlurLeft, imgLeft, mapx1, mapy1, INTER_LINEAR);  
    remap(gaussianBlurRight, imgRight, mapx2, mapy2, INTER_LINEAR);
	//imshow("ImageL_Rectified",imgLeft);
    //imshow("ImageR_Rectified",imgRight);
}
void show_rectify_performance()
{
	/**********************************************************************************/
    /***************������ͼ���У�������ʾ��ͬһ�����Ͻ��жԱ�*********************/ 
    Mat canvas;  
    double sf=0.7;  
    int w, h;   
    w = cvRound(imageSize.width * sf);  
    h = cvRound(imageSize.height * sf);  
    canvas.create(h, w * 2, CV_8UC1);  
    //��ͼ�񻭵�������
	//�õ���������벿�� 
    Mat canvasPart = canvas(Rect(w*0, 0, w, h));  
	//��ͼ�����ŵ���canvasPartһ����С��ӳ�䵽����canvas��ROI������  
    resize(imgLeft, canvasPart, canvasPart.size(), 0, 0, INTER_AREA);     
    //��ͼ�񻭵������� 
    canvasPart = canvas(Rect(w, 0, w, h)); 
    resize(imgRight, canvasPart, canvasPart.size(), 0, 0, INTER_AREA);  
    //���϶�Ӧ������
    for (int i = 0; i < canvas.rows;i+=16)  
        line(canvas, Point(0, i), Point(canvas.cols, i), Scalar(0, 255, 0), 1, 8);  
    imshow("rectified", canvas);
	/**********************************************************************************/
}
bool getDisparityRGBImage(cv::Mat& disparity, cv::Mat& disparityRGBImage, bool isColor)
{
	// ��ԭʼ�Ӳ����ݵ�λ��ת��Ϊ 8 λ
	cv::Mat disp8u;
	if (disparity.depth() != CV_8U)
	{
		disparity.convertTo(disp8u, CV_8U, 255.0/(numDisparities*16));
	} 
	else
	{
		disp8u = disparity;
	}

	// ת��Ϊα��ɫͼ�� �� �Ҷ�ͼ��
	if (isColor)
	{
		if (disparityRGBImage.empty() || disparityRGBImage.type() != CV_8UC3 )
		{
			disparityRGBImage = cv::Mat::zeros(disparity.rows, disparity.cols, CV_8UC3);
		}

		for (int y=0;y<disparity.rows;y++)
		{
			for (int x=0;x<disparity.cols;x++)
			{
				uchar val = disp8u.at<uchar>(y,x);
				uchar r,g,b;

				if (val==0) 
					r = g = b = 0;
				else
				{
					r = 255-val;
					g = val < 128 ? val*2 : (uchar)((255 - val)*2);
					b = val;
				}

				disparityRGBImage.at<cv::Vec3b>(y,x) = cv::Vec3b(r,g,b);
			}
		}
	} 
	else
	{
		disp8u.copyTo(disparityRGBImage);
	}

	return true;
}
void stereo_match_BM(int,void*)
{
	//minDisparity������ƥ�����������￪ʼ,Ĭ��ֵΪ 0
	//numberOfDisparities��ʾ��������ӲΧ
	//�����ӣ�Ҫ����ͼ��������ͼ�еĵ㣨x0,y0����ƥ��㣬������ʱ����ͼ�еģ�x0,y0������ԭ��
	//������������Ϊ����������ΧΪminDisparity-numberOfDisparities��
	//��minDisparity=0����ʾ����ͼ�еĵ㣨x0,y0����ʼ��������
	//��minDisparity<0����ʾ����ͼ�еĵ㣨x0-minDisparity,y0����ʼ��������
	//blockSize��ʾƥ�䴰�ڴ�С������Խ��ƥ������³����Խǿ�����Ǿ���Խ���֮
	//����״̬��������ʹ����Ĭ��ֵ
	//opencv3.0�汾�£�StereoBM������ֱ��ͨ������state�����ʲ�����
	//ֻ��ͨ��setter��getter���������úͻ�ȡ����

	bm->setUniquenessRatio(UniquenessRatio);//�Ӳ�Ψһ�԰ٷֱ�
	bm->setNumDisparities(16*numDisparities+16);
	bm->setBlockSize(2*blockSize+5);
	//bm->setTextureThreshold(10);//������������ж���ֵ
	//bm->setPreFilterType(CV_STEREO_BM_NORMALIZED_RESPONSE);//CV_STEREO_BM_NORMALIZED_RESPONSE  CV_STEREO_BM_XSOBEL
	//bm->setPreFilterSize(9);//Ԥ�����˲������ڴ�С,һ����[5��21]֮��
	//bm->setPreFilterCap(31);//Ԥ�����˲����Ľض�ֵ,������Χ��1 - 31���ĵ�����31������������ 63��

	//�����ȡ�Ӳ�ͼ
	bm->compute(imgLeft, imgRight, disparity);

	//ע��opencv3.0�汾�µ�BM��SGBM������������Ӳ��CV_16S��ʽ��
	//�ο����ϸ���Դ�������ݣ��õ���ԭʼ�Ӳ�ֵ��Ҫ����16���ܵõ���ʵ���Ӳ�ֵ
	disparity.convertTo(disparity_real, CV_8U, 1.0/16);

	//���õ����Ӳ�ֵ��Χ��minDisparity-��minDisparity+numDisparities����ͶӰ��(0-255)
	disparity_real.convertTo(disparity8U, CV_8U, 255.0/numDisparities);
		
	//imshow("disparity",disparity);
	imshow("disparity_real",disparity_real);
	//imshow("disparity_8U",disparity8U);
	//����isColor��ֵѡ���Ƿ�������ͼα��ɫ��ʾ
	Mat disparityRGBImage;
	bool isColor=false;
	bool OK=getDisparityRGBImage(disparity,disparityRGBImage,isColor);
	if(OK==true&&isColor==true)
	{
		imshow("disparityRGBImage",disparityRGBImage);
	}

	//-- Check its extreme values
	double minVal; double maxVal;
	minMaxLoc( disparity_real, &minVal, &maxVal );

	cout<<"Min disp:"<< minVal<<endl;
	cout<<"Max value:"<<maxVal<<endl;
}

int main(int argc, char* argv[])
{
	/***********************************************************************************/
	/*****************************���ļ��ж�ȡ����궨���******************************/
	const string filename="F:\\Binocular_Stereo_Vision_Test\\Calibration_Result.xml";
	bool readOK=readFile(filename);
	if(!readOK)
	{
		cerr<<"failed to readfile!"<<endl;
		return -1;
	}
	/**********************************************************************************/
	/********************************����У��******************************************/
	bool useCalibrated=true; 
	//int getImagePair=IMAGELIST;
	int getImagePair=CAMERA;
	string path1="Stereo_Sample\\ImageL1.jpg";
	string path2="Stereo_Sample\\ImageR1.jpg";

	stereo_rectify(useCalibrated,getImagePair,path1,path2);
	if(getImagePair==CAMERA&&isopen==false)
	{
		cerr<<"failed to open camera to get imagepair!"<<endl;
		waitKey(5000);
		return -1;
	}
    //������ͼ���У�������ʾ��ͬһ�����Ͻ��жԱ�
    bool showRectifyPerformance=true;
	if(showRectifyPerformance)
	{
		show_rectify_performance();
	}

	/**********************************************************************************/
	/********************************����ƥ��******************************************/
	//ͨ�����û��������Թ۲�������Ҫ�����ı仯��ƥ��Ч����Ӱ��
	namedWindow("disparity_real", WINDOW_AUTOSIZE);
	bm->compute(imgLeft, imgRight, disparity);
	//imshow("disparity_real",disparity);
    createTrackbar("blockSize:", "disparity_real",&blockSize,8, stereo_match_BM);
    createTrackbar("UniquenessRatio:", "disparity_real", &UniquenessRatio,15, stereo_match_BM);
    createTrackbar("numDisparities:", "disparity_real", &numDisparities,25, stereo_match_BM);
	

	/**********************************************************************************/
	/********************************��ά�ؽ�******************************************/
	//�������ص�����on_mouse()����������ָ���㣨������������Ӳ�ֵ����άXYZֵ����λ�����ף�
	cvSetMouseCallback("disparity_real",on_mouse,NULL);
	while(1)
	{
		if(mousePoint.area()>0)
		{
			//�����ָ�����ĵ���ԲȦ��ǳ���
			circle(disparity_real,mousePoint,10,Scalar(255,255,255),3);
			imshow("disparity_real",disparity_real);

			uchar *data=disparity_real.ptr<uchar>(mousePoint.height);
			cout<<"disparity="<<(double)data[mousePoint.width]<<endl;
			//cout<<"disparity="<<disparity.at<double>(mousePoint)<<endl;
			_3dImage=Mat( disparity.rows, disparity.cols, CV_32FC3);
			reprojectImageTo3D(disparity,_3dImage,Q,true);
			//��ʵ�������ʱ��ReprojectTo3D������X / W, Y / W, Z / W��Ҫ����16
			//���ܵõ���ȷ����ά������Ϣ(�Ժ���Ϊ��λ),Ҫ�õ�������Ϊ��λ����ά���������1.6
			_3dImage=_3dImage*1.6;//�õ�������Ϊ��λ����ά����
			//imshow("��άͼ",_3dImage);
			//ReprojectTo3D()����������Y���귽���ʵ���෴����Ҫ����һ��
			for (int y = 0; y < _3dImage.rows; ++y)
			{
				for (int x = 0; x < _3dImage.cols; ++x)
				{
					Point3f point = _3dImage.at<Point3f>(y,x);
					point.y = -point.y;
					_3dImage.at<Point3f>(y,x) = point;
				}
			}
			X=_3dImage.at<Vec3f>(mousePoint)[0];
			Y=_3dImage.at<Vec3f>(mousePoint)[1];
			Z=_3dImage.at<Vec3f>(mousePoint)[2];
			cout<<"X="<<X<<'\t'<<"Y="<<Y<<'\t'<<"Z="<<Z<<endl;
			mousePoint.width=0;
			mousePoint.height=0;
		}
		if((char)waitKey(10)==27)
			break;
	}
	/*
	vector<Mat> xyz;
	split(_3dImage,xyz);
	double minVal; double maxVal;
	minMaxLoc( xyz[0], &minVal, &maxVal );
	xyz[0].convertTo(xyz[0],CV_8UC1,255/(maxVal - minVal));
	minMaxLoc( xyz[1], &minVal, &maxVal );
	xyz[1].convertTo(xyz[1],CV_8UC1,255/(maxVal - minVal));
	minMaxLoc( xyz[2], &minVal, &maxVal );
	xyz[2].convertTo(xyz[2],CV_8UC1,255/(maxVal - minVal));
	//Mat _3dImage1=Mat( disparity.rows, disparity.cols, CV_8UC3 );
	//merge(xyz,_3dImage);
	imshow("xImage",xyz[0]);
	imshow("yImage",xyz[1]);
	imshow("zImage",xyz[2]);
	//imshow("_3dImage",_3dImage);
	*/
	cout<<"��������˳�����..."<<endl;
	waitKey(0);
	return 0;
}
#include "Scanner_implementation.hpp"

#include <SgSystem.h>
#include <SgPoint.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <cmath>
#include <fstream>

namespace Go_Scanner {

cv::Mat img0, selectedImg, temp;
void releaseImg(cv::Mat a,int x,int y);
void showImage();
cv::Mat holdImg(int x,int y);

std::string windowName = "Augmented Go Cam";

bool debug = false;
bool showall = false;

bool asked_for_board_contour = false;

/*

Rectangle Order: 
0-------1
|        |
|        |
3-------2

*/

//point coordinates
int boardCornerX[]={80, 443, 460, 157};    
int boardCornerY[]={80, 87, 430, 325};
int imagewidth;
int imageheight;


int point=-1;            //currently selected point
int nop=4;                //number of points

enum lineType{HORIZONTAL, VERTICAL};
enum stoneColor{BLACK, WHITE};
enum lineheading{LEFT, RIGHT};
enum LineCoord{START_X=0, START_Y=1, END_X=2, END_Y=3};

struct PartitionOperator
{
    int sidesize;

    PartitionOperator(int size)
    {
        sidesize = size;
    }

    bool operator()(const int &a, const int &b) const 
    {
        float distance = a - b;
        distance = abs(distance);

        float boardfield = ((1.0f/18.0f) * sidesize)/3.0f;

        return distance < boardfield;
    }
};

void mouseHandler(int event, int x, int y, int flags, void *param)
{

    switch(event) 
    {
    case CV_EVENT_LBUTTONDOWN:        
        selectedImg = holdImg(x, y);
        break;

    case CV_EVENT_LBUTTONUP:    
        if((selectedImg.empty()!= true)&& point!=-1)
        {
            releaseImg(selectedImg,x,y);
            selectedImg=cv::Mat();
        }
        break;

    //Draws the lines while navigating with the mouse
    case CV_EVENT_MOUSEMOVE:
        /* draw a rectangle*/
        if(point!=-1)
        {
            if(selectedImg.empty()!= true)
            {
                temp = selectedImg.clone();
                cv::rectangle(temp, 
                    cv::Point(x - 10, y - 10), 
                    cv::Point(x + 10, y + 10), 
                    //BGR not RGB!!
                    cv::Scalar(0, 255, 0, 0), 2, 8, 0);

                //adjust the lines
                for(int i=0;i<nop;i++)
                {
                    if(i!=point)
                    {
                        cv::line(temp,
                            cv::Point(x, y), 
                            cv::Point(boardCornerX[i] , boardCornerY[i] ), 
                            cv::Scalar(0, 255, 0 ,0), 1,8,0);
                    }
                }
                cv::imshow(windowName, temp); 
            }
            break;
        }
        temp = cv::Mat();
    }
}

//draws the lines and points while holding left mouse button down
cv::Mat holdImg(int x, int y)
{
    cv::Mat img = img0;

    int radius = 4;
    //find what point is selected
    for(int i=0;i<nop;i++){
        if((x>=(boardCornerX[i]-radius)) && (x<=(boardCornerX[i]+radius ))&& (y<=(boardCornerY[i]+radius ))&& (y<=(boardCornerY[i]+radius ))){
            point=i;
            break;
        }

    }
    //draw points
    for(int j=0;j<nop;j++)
    {
        //if this is not the selected point
        if(j!=point)
        {
            img = img.clone();
            cv::rectangle(img, 
                cv::Point(boardCornerX[j] - 1, boardCornerY[j] - 1), 
                cv::Point(boardCornerX[j] + 1, boardCornerY[j] + 1), 
                cv::Scalar(255, 0,  0, 0), 2, 8, 0);
        }
    }

    //draw lines
    for(int i=0;i<nop;i++)
    {
        if(i!=point)
        {
            for(int k=i+1;k<nop;k++)
            {
                if(k!=point)
                {
                    img = img.clone();
                    cv::line(img,
                        cv::Point(boardCornerX[i] , boardCornerY[i] ), 
                        cv::Point(boardCornerX[k] , boardCornerY[k] ), 
                        cv::Scalar(255, 0, 0, 0), 1,8,0);

                }
            }
        }
    }
    return img;
}

//set new coordinates and redraw the scene if the left mouse button is released
void releaseImg(cv::Mat a, int x, int y)
{
    boardCornerX[point]=x;
    boardCornerY[point]=y;
    showImage();
}

/**
 * @brief   Shows the selected points (through manual or automatic board detection) on a clone of the img0 in a new window.
 */
void showImage()
{
    cv::Mat img1 = img0.clone();

    //draw the points
    for(int j=0;j<nop;j++)
    {        
        cv::rectangle(img1, 
            cv::Point(boardCornerX[j] - 1, boardCornerY[j] - 1), 
            cv::Point(boardCornerX[j] + 1, boardCornerY[j] + 1), 
            cv::Scalar(255, 0,  0, 0), 2, 8, 0);


        //draw the lines
        for(int k=j+1;k<nop;k++)
        {
            cv::line(img1,
                cv::Point(boardCornerX[j] , boardCornerY[j] ), 
                cv::Point(boardCornerX[k] , boardCornerY[k] ), 
                cv::Scalar(255, 0,  0, 0), 1,8,0);
        }
    }
    cv::imshow(windowName, img1);
}

cv::Mat warpImage(cv::Mat img, cv::Point2f p0, cv::Point2f p1, cv::Point2f p2, cv::Point2f p3)
{
    /*
    Rectangle Order: 
    0-------1
    |        |
    |        |
    2-------3
    */
    cv::Point2f selCorners[4];        
    selCorners[0] = p0;
    selCorners[1] = p1;
    selCorners[2] = p2;
    selCorners[3] = p3;

    cv::Point2f dstCorners[4]; 
    dstCorners[0] = cv::Point2f(0.0, 0.0);
    dstCorners[1] = cv::Point2f((float)img.cols, 0.0);
    dstCorners[2] = cv::Point2f(0.0, (float)img.rows);
    dstCorners[3] = cv::Point2f((float)img.cols, (float)img.rows);

    cv::Mat transformationMatrix;
    transformationMatrix = cv::getPerspectiveTransform(selCorners, dstCorners);

    cv::Mat warpedImg;
    cv::warpPerspective(img, warpedImg, transformationMatrix, warpedImg.size(), 1, 0 ,0);

    return warpedImg;
}

//cv::Mat warpImage()
//{
//    /*
//    Rectangle Order: 
//    0-------1
//    |        |
//    |        |
//    2-------3
//    */
//    cv::Point2f selCorners[4];        
//    selCorners[0] = cv::Point2f(boardCornerX[0], boardCornerY[0]);
//    selCorners[1] = cv::Point2f(boardCornerX[1], boardCornerY[1]);
//    selCorners[2] = cv::Point2f(boardCornerX[3], boardCornerY[3]);
//    selCorners[3] = cv::Point2f(boardCornerX[2], boardCornerY[2]);
//
//    for (int i=0; i < 4; i ++)
//        std::cout << boardCornerX[i] << " " << boardCornerY[i] << std::endl;
//
//    cv::Point2f dstCorners[4]; 
//    dstCorners[0] = cv::Point2f(0.0, 0.0);
//    dstCorners[1] = cv::Point2f(imagewidth-1, 0.0);
//    dstCorners[2] = cv::Point2f(0.0, imageheight-1);
//    dstCorners[3] = cv::Point2f(imagewidth-1, imageheight-1);
//
//
//    cv::Mat transformationMatrix;
//    transformationMatrix = cv::getPerspectiveTransform(selCorners, dstCorners);
//
//    cv::Mat warpedImg;
//    cv::warpPerspective(img0, warpedImg, transformationMatrix, warpedImg.size(), 1, 0 ,0);
//
//    return warpedImg;
//}

cv::Mat sobelFiltering(cv::Mat warpedImgGray)
{
    //Sobel Filtering
    cv::Mat grad;
    cv::Mat grad_x, grad_y;
    cv::Mat abs_grad_x, abs_grad_y;
    int scale = 1;
    int delta = 0;
    int ddepth = CV_16S;

    /// Gradient X
    cv::Sobel( warpedImgGray, grad_x, ddepth, 1, 0, 3, scale, delta, 0);
    /// Gradient Y
    cv::Sobel( warpedImgGray, grad_y, ddepth, 0, 1, 3, scale, delta, 0);

    cv::convertScaleAbs( grad_x, abs_grad_x );
    cv::convertScaleAbs( grad_y, abs_grad_y );
    cv::addWeighted( abs_grad_x, 0.5, abs_grad_y, 0.5, 0, grad );

    return grad;
}

cv::Mat laplaceFiltering(cv::Mat warpedImgGray)
{
    cv::Mat abs_dst;
    cv::Mat dst;
    int kernel_size = 3;
    int scale = 1;
    int delta = 0;
    int ddepth = CV_16S;

    cv::Laplacian( warpedImgGray, dst, ddepth, kernel_size, scale, delta, 0);
    convertScaleAbs(dst, abs_dst);

    return abs_dst;
}

float calcBetweenAngle(cv::Vec2f v1, cv::Vec2f v2)
{
    float angle;

    //calculate angle in radians between the 2 vectors
    angle = acosf((v1[0]*v2[0] + v1[1]*v2[1])/(sqrtf((v1[0]*v1[0])+(v1[1]*v1[1]))*sqrtf((v2[0]*v2[0])+(v2[1]*v2[1]))));
    
    //to degree
    angle = angle * (180.0f/3.14159f);

    return angle;
}

void groupIntersectionLines(cv::vector<cv::Vec4i>& lines, cv::vector<cv::Vec4i>& horizontalLines, cv::vector<cv::Vec4i>& verticalLines)
{
    cv::Vec2f baseVector, lineVector;

    //baseVector
    baseVector[0] = imagewidth;
    baseVector[1] = 0;

    for (int i = 0; i < lines.size(); i++)
    {
        lineVector[0] = lines[i][2] - lines[i][0];
        lineVector[1] = lines[i][3] - lines[i][1];

        float angle = calcBetweenAngle(baseVector, lineVector);
        if(angle != 0.0f && angle != 90.0f)
            std::cout << angle << std::endl;

        //horizontal lines
        if(angle <= 1.0f && angle >= -1.0f)
        {
            cv::Vec4i v = lines[i];
            lines[i][0] = 0;
            lines[i][1] = ((float)v[1] - v[3]) / (v[0] - v[2]) * -v[0] + v[1]; 
            lines[i][2] = imagewidth; 
            lines[i][3] = ((float)v[1] - v[3]) / (v[0] - v[2]) * (imagewidth - v[2]) + v[3];
            
            horizontalLines.push_back(lines[i]);
        }

        //vertical lines
        else if(angle >= 88.0f && angle <= 92.0f)
        {
            cv::Vec4i v = lines[i];

            //Problem: For completely vertical lines, there is no m 
            // y = m*x+n

            if((v[0] - v[2]) != 0)
            {
                float m = (v[1] - v[3]) / (v[0] - v[2]);
                float n = v[1] - m * v[0];

                lines[i][0] = (-n)/m;               //x_start         
                lines[i][1] = 0;                    //y_start
                lines[i][2] = (imageheight-n)/m;    //x_end
                lines[i][3] = imageheight;          //y_end
            }
            else
            {
                lines[i][0] = v[0];                 //x_start         
                lines[i][1] = 0;                    //y_start
                lines[i][2] = v[2];                 //x_end
                lines[i][3] = imageheight;          //y_end
            }
            verticalLines.push_back(lines[i]);
        }
        //other lines
        else
        {
            lines.erase(lines.begin()+i);
            std::cout << "This Line is deleted. Muahaha" << std::endl;
            //Delete that Line. Its a false line :)
        }
    }

}

cv::vector<cv::Vec4i> getBoardLines(cv::vector<cv::Vec4i>& lines, lineType type)
{
    int valueIndex1, valueIndex2, imagesizeIndex, zeroIndex, imagesize;

    if(type == VERTICAL)
    {
        valueIndex1 = 0;            //0 = line[0] = x_start
        valueIndex2 = 2;            //2 = line[2] = x_end
        imagesizeIndex = 3;         //3 = line[3] = y_end
        zeroIndex = 1;              //1 = line[1] = y_start
        imagesize = imageheight;
    }
    else if (type == HORIZONTAL)
    {
        valueIndex1 = 1;
        valueIndex2 = 3;        
        imagesizeIndex = 2;
        zeroIndex = 0;
        imagesize = imagewidth;
    }

    //Put the Starting Points and Ending Points of a Line into the lineStarts and lineEnds Vector
    cv::vector<int> lineStarts(lines.size());
    cv::vector<int> lineEnds(lines.size());

    for(size_t i=0; i<lines.size(); i++)
    {
        lineStarts[i] = lines[i][valueIndex1];
        lineEnds[i] = lines[i][valueIndex2];
    }

    //clustering of linedata. Creating the Clusters with the PartitionOperator and store them into ClusterNum.
    cv::vector<int> clusterNum(lines.size());
    PartitionOperator Oper(imagesize);
    int clusterSize = cv::partition<int, PartitionOperator>(lineStarts, clusterNum, Oper);


    //put the lines in there cluster
    cv::vector<cv::vector<int>> clusteredLineStarts(clusterSize);
    cv::vector<cv::vector<int>> clusteredLineEnds(clusterSize);

    for(size_t i = 0; i < clusterNum.size(); i++)
    {
        int j = clusterNum[i];

        clusteredLineStarts[j].push_back(lineStarts[i]);
        clusteredLineEnds[j].push_back(lineEnds[i]);
    }

    // middle the lines
    cv::vector<cv::Vec4i> middledLines;
    cv::Vec4i middle;

    for(int i=0; i < clusterSize; i++)
    {
        int numLines = clusteredLineStarts[i].size();
        int sumStarts=0;
        int sumEnds=0;

        for(int j=0; j < numLines; j++)
        {
            sumStarts += clusteredLineStarts[i][j];
            sumEnds += clusteredLineEnds[i][j];
        }

        int midStarts = sumStarts/numLines;
        int midEnds = sumEnds/numLines;

        middle[zeroIndex] = 0;
        middle[valueIndex1] = midStarts;
        middle[imagesizeIndex] = imagesize;
        middle[valueIndex2] = midEnds;

        middledLines.push_back(middle);
    }

    return middledLines;
}

bool intersection(cv::Vec4i horizontalLine, cv::Vec4i verticalLine, cv::Point2f &r)
{
    cv::Point2f o1;
    o1.x = horizontalLine[0];
    o1.y = horizontalLine[1];
    cv::Point2f p1;
    p1.x = horizontalLine[2];
    p1.y = horizontalLine[3];
    cv::Point2f o2;
    o2.x = verticalLine[0];
    o2.y = verticalLine[1];
    cv::Point2f p2;
    p2.x = verticalLine[2];
    p2.y = verticalLine[3];

    cv::Point2f x = o2 - o1;
    cv::Point2f d1 = p1 - o1;
    cv::Point2f d2 = p2 - o2;

    float cross = d1.x*d2.y - d1.y*d2.x;
    if (abs(cross) < /*EPS*/1e-8)
        return false;

    double t1 = (x.x * d2.y - x.y * d2.x)/cross;
    r = o1 + d1 * t1;
    if(r.x>=imagewidth || r.y>=imageheight)
        return false;

    return true;
}

bool getBoardIntersections(cv::Mat warpedImg, int thresholdValue, cv::vector<cv::Point2f> &intersectionPoints, cv::Mat& paintedWarpedImg)
{
    /*TODO: Find the best results
        Canny + HoughLinesP gives the best results.

        Dont know if the filtering methods need to be used. 

        threshold will be implemented for bad images

        sort the lines! 

        what if there are not enough intersections points? 
        what if there are to many intersections points? 

        if intersections is not on a black or white point. get him to one. 
    */

    //Set the right Threshold for the image
    cv::Mat warpedImgGray, threshedImg, sobelImg, cannyImg;
    int const maxValue = 255;
    int thresholdType = 4;

    //reduce the noise
    //cv::blur(warpedImg, warpedImg , cv::Size(3,3));
    cv::cvtColor(warpedImg, warpedImgGray, CV_RGB2GRAY);
    cv::imshow("blur", warpedImgGray);

    cv::Canny(warpedImgGray, cannyImg, 100, 150, 3);
    cv::imshow("Canny", cannyImg);

    //sobelImg = sobelFiltering(cannyImg);
    cv::threshold(cannyImg, threshedImg, 255, maxValue, thresholdType );

    cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
    cv::morphologyEx(threshedImg, threshedImg, cv::MORPH_CLOSE, element); // Apply the specified morphology operation

    cv::imshow("Threshed Image for HoughLinesP", threshedImg);

    cv::vector<cv::Vec4i> lines, horizontalLines, verticalLines;
    cv::HoughLinesP(threshedImg, lines, 1, CV_PI/180, 80, 10, 5);

    cv::Mat houghimage = warpedImg.clone();

    //Draw the lines
    /*
        Structure of line Vector:

        lines[i][0] = x_start         
        lines[i][1] = y_start
        lines[i][2] = x_end
        lines[i][3] = y_end      
    */
    for( size_t i = 0; i < lines.size(); i++ )
    {
        cv::line(houghimage, cv::Point(lines[i][0], lines[i][1]),
            cv::Point(lines[i][2], lines[i][3]), cv::Scalar(0,0,255), 1, 8 );
    }

    cv::imshow("HoughLines Image", houghimage);

    groupIntersectionLines(lines, horizontalLines, verticalLines);
    // if horizontalLines.size() == 0; net aufrufen

    cv::vector<cv::Vec4i> newhorizontalLines = getBoardLines(horizontalLines, HORIZONTAL);
    cv::vector<cv::Vec4i> newverticalLines = getBoardLines(verticalLines, VERTICAL);

    //get the intersections
    for(size_t i=0; i < newhorizontalLines.size();  i++)
    {
        for(size_t j=0; j < newverticalLines.size(); j++)
        {
            cv::Point2f intersectedPoint;
            bool result = intersection(newhorizontalLines[i], newverticalLines[j], intersectedPoint);

            if(result == true)
                intersectionPoints.push_back(intersectedPoint);
        }
    }

    cv::vector<cv::Vec4i> newLines; 

    newLines.insert(newLines.begin(), newhorizontalLines.begin(), newhorizontalLines.end());
    newLines.insert(newLines.end(), newverticalLines.begin(), newverticalLines.end());

    //Draw the lines
    for( size_t i = 0; i < newLines.size(); i++ )
    {
        cv::line(paintedWarpedImg, cv::Point(newLines[i][0], newLines[i][1]),
            cv::Point(newLines[i][2], newLines[i][3]), cv::Scalar(0,0,255), 1, 8 );
    }

    //Draw the intersections
    for(size_t i= 0; i < intersectionPoints.size(); i++)
    {
        cv::rectangle(paintedWarpedImg, 
        cv::Point(intersectionPoints[i].x-1, intersectionPoints[i].y-1),
        cv::Point(intersectionPoints[i].x+1, intersectionPoints[i].y+1), 
        cv::Scalar(255, 0,  0, 0), 2, 8, 0);
    }


    //cv::imshow("Intersections", warpedImg);
    
    return true;
}

int getStoneDistanceAndMidpoint(const cv::Mat& warpedImgGray, int x, int y, lineheading heading, cv::Point2f& midpointLine)
{

    /**
    *    Returns the size of a 45�(left headed) or 135�(right headed) line within a circle, 
    *    starting from any point within that circle 
    *    mainly used to get the diameters of the stones. 
    *    This functin also provides the Midpoint of that line. 
    */

    int d1=0, d2=0, distance, xTmp1, yTmp1, xTmp2, yTmp2;

    if(heading == RIGHT)
    {    
        xTmp1 = x;
        yTmp1 = y;
        while (xTmp1 > 0 && xTmp1 < imagewidth
            && yTmp1 > 0 && yTmp1 < imageheight
            && warpedImgGray.at<uchar>(yTmp1,xTmp1) < 50)
        {
            //runs to bottom left
            xTmp1 -= 1;    
            yTmp1 += 1; 
            d1++;
        }

        xTmp2 = x;
        yTmp2 = y;
        while(xTmp2 > 0 && xTmp2 < imagewidth
            && yTmp2 > 0 && yTmp2 < imageheight
            && warpedImgGray.at<uchar>(yTmp2,xTmp2) < 50)
        {
            //runs to top right
            xTmp2 += 1;
            yTmp2 -= 1; 
            d2++;
        }
    }
    else //heading is left
    {

        xTmp1 = x;
        yTmp1 = y;
        while(xTmp1 > 0 && xTmp1 < imagewidth
            && yTmp1 > 0 && yTmp1 < imageheight
            && warpedImgGray.at<uchar>(yTmp1,xTmp1) < 50)
        {
            //runs to top left
            xTmp1 -= 1;    
            yTmp1 -= 1; 
            d1++;
        }

        xTmp2 = x;
        yTmp2 = y;
        while(xTmp2 > 0 && xTmp2 < imagewidth
            && yTmp2 > 0 && yTmp2 < imageheight
            && warpedImgGray.at<uchar>(yTmp2,xTmp2) < 50)
        {
            //runs to bottom right
            xTmp2 += 1;
            yTmp2 += 1; 
            d2++;
        }
    }


    //distance of our line
    distance = d1 + d2;

    // midpoint of our first help line
    midpointLine.x = (xTmp1 + xTmp2) / 2.0f;
    midpointLine.y = (yTmp1 + yTmp2) / 2.0f;

    return distance;
}

void detectBlackStones(cv::Mat& warpedImg, cv::vector<cv::Point2f> intersectionPoints, std::map<cv::Point2f, SgPoint, lesserPoint2f> to_board_coords, SgPointSet& stones, cv::Mat& paintedWarpedImg)
{
    //TODO: use black/white image. write function for it. 
    
    cv::Mat tmp, warpedImgGray;
    cv::cvtColor(warpedImg, tmp, CV_RGB2GRAY);

    cv::threshold(tmp, warpedImgGray, 85, 255, 0);

    cv::imshow("Image for detecting black stones", warpedImgGray);
    for(int i=0; i < intersectionPoints.size(); i++)
    {
        auto& intersection_point = intersectionPoints[i];

        int x = intersectionPoints[i].x;
        int y = intersectionPoints[i].y;
        int distance, diameter45, diameter125;

        /**
        * Let's check if this is a stone :). We'll read out the diameters at 45� and 125� if they 
        * are similar -> it's a stone. 
        */

        if (warpedImgGray.at<uchar>(y,x) < 20)
        {

            /**    
            *    first we produce a help line to get the diameters at 45� and 125�.
            *    therefore we use the intersectionpoints. 
            */ 
            cv::Point2f midpointLine;
            distance = getStoneDistanceAndMidpoint(warpedImgGray, x, y, LEFT, midpointLine);

            // Get the Diameter for 125 degree
            cv::Point2f midpoint125;
            diameter125 = getStoneDistanceAndMidpoint(warpedImgGray, midpointLine.x, midpointLine.y, RIGHT, midpoint125);

            // Get the Diameter for 45 degree
            cv::Point2f midpoint45;
            diameter45 = getStoneDistanceAndMidpoint(warpedImgGray, midpoint125.x, midpoint125.y, LEFT, midpoint45);

            if(diameter125+5 >= diameter45 && diameter125-5 <= diameter45 && diameter45 >= 10 && diameter125 >= 10  )
            {
                std::cout << "Black Stone ("<< x << ", "<< y << ")" << std::endl;

                stones.Include(to_board_coords[intersection_point]);

                cv::circle( paintedWarpedImg, 
                cv::Point(midpoint125.x, midpoint125.y),
                (diameter125/2), 
                cv::Scalar(0, 255, 0), 0, 8, 0);
            }
        }
    }
}

void detectAllStones(cv::Mat& warpedImg, cv::vector<cv::Point2f> intersectionPoints, std::map<cv::Point2f, SgPoint, lesserPoint2f> to_board_coords, SgPointSet& stones, float stone_diameter, cv::Mat& paintedWarpedImg)
{   
    using namespace cv;
    cv::Mat img;

    cv::cvtColor(warpedImg, img, CV_RGB2GRAY);
    cv::Canny(img, img, 100, 150, 3);

    // After canny we get a binary image, where all detected edges are white.
    // Dilate iterates over ervery pixel, and sets the pixel to the maximum pixel value within a circle around that pixel.
    // This leaves the areas black where no edges were detected and thus could contain a stone.
    Mat element_dilate = getStructuringElement(MORPH_ELLIPSE, Size(stone_diameter*0.8+0.5f, stone_diameter*0.8+0.5f));
    cv::dilate(img, img, element_dilate);
    cv::imshow("after dilating", img);

    // Only the intersection points (later accessed by img.at<uchar>(y,x)) are relevant.
    // Erode looks for the minimum pixel value (black) within a circle around every pixel.
    // Because black pixels represent possible stones, if there is any black pixel within 
    // a stone radius around an intersection then we have found a stone.
    Mat element_erode = getStructuringElement(MORPH_ELLIPSE, Size(stone_diameter*0.6+0.5f, stone_diameter*0.6+0.5f));
    // todo(mihi314) could be optimized by not using erode and instead only doing the erode operation at intersection points
    cv::erode(img, img, element_erode);

    cv::imshow("Image for detecting white stones", img);

    for(int i=0; i < intersectionPoints.size(); i++)
    {
        auto& intersection_point = intersectionPoints[i];

        int x = intersection_point.x + 0.5f;
        int y = intersection_point.y + 0.5f;
        int distance, diameter45, diameter125;

        /**
        * Let's check if this is a stone :). 
        */
        if (img.at<uchar>(y,x) < 50)
        {
            std::cout << "White Stone ("<< x << ", "<< y << ")" << std::endl;
            stones.Include(to_board_coords[intersection_point]);

            cv::circle( paintedWarpedImg, 
            cv::Point(intersectionPoints[i].x, intersectionPoints[i].y),
            (stone_diameter/2.0f), 
            cv::Scalar(238, 238, 176), 0, 8, 0);

        }
    }
}


    cv::Mat Thresh;
    cv::Mat Thresh_res;
    int threshold_value = 100;
    int threshold_type = 0;
    int const max_value = 255;
    int const max_type = 4;
    int const max_BINARY_value = 255;
    
    char* window_name_thre = "Threshold Demo";
    char* trackbar_type = "Type: \n 0: Binary \n 1: Binary Inverted \n 2: Truncate \n 3: To Zero \n 4: To Zero Inverted";
    char* trackbar_value = "Value";

void Threshold_Debug( int, void* )
{
  /* 0: Binary
     1: Binary Inverted
     2: Threshold Truncated
     3: Threshold to Zero
     4: Threshold to Zero Inverted
   */
    cv::Mat sobel;

    //sobel = sobelFiltering(Thresh);

    cv::Mat sub_mat = cv::Mat::ones(Thresh.size(), Thresh.type())*255;
    //subtract the original matrix by sub_mat to give the negative output new_image

    cv::subtract(sub_mat, Thresh, sobel);

    cv::threshold(sobel, Thresh_res, threshold_value, max_BINARY_value,threshold_type );

    cv::imshow( window_name_thre, Thresh_res );
}

/**
 * @brief   Calls the manual board detection and shows the result in a new window.
 */
void ask_for_board_contour() {
    cv::namedWindow(windowName, CV_WINDOW_AUTOSIZE);
    cv::setMouseCallback(windowName, mouseHandler, NULL);
    cv::putText(img0, "Mark the Go board with the blue rectangle. Press any Key when finished.", 
                cvPoint(20,20), 1, 0.8, cv::Scalar(64, 64, 255), 1, CV_AA);
    showImage();
    cv::waitKey(0);
    cv::destroyWindow(windowName);

    asked_for_board_contour = true;
}

/**
 * @brief   Calls the automatic board detection and shows the result in a new window.
 *          Prints an error to the console if the automatic detection couldn't find anything.
 */
void do_auto_board_detection() {
    cv::Point2f p0, p1, p2, p3;
    automatic_warp(img0, p0, p1, p2, p3);

    // automatic_warp failed and board wasn't found
    // if one of the points wasn't set
    if (p0 == cv::Point2f()) {
        std::cout << "!!ERROR >> Failed to automatically detect the go board!" << std::endl;
        return;
    }

    // HACK: adjust the coordinates a bit to snap them closer to the board grid
    const auto gap = .5f;
    p0 += cv::Point2f(gap, gap);
    p1 += cv::Point2f(-gap, gap);
    p2 += cv::Point2f(-gap, -gap);
    p3 += cv::Point2f(gap, -gap);

    boardCornerX[0] = p0.x;
    boardCornerX[1] = p1.x;
    boardCornerX[2] = p2.x;
    boardCornerX[3] = p3.x;

    boardCornerY[0] = p0.y;
    boardCornerY[1] = p1.y;
    boardCornerY[2] = p2.y;
    boardCornerY[3] = p3.y;

    cv::namedWindow(windowName, CV_WINDOW_AUTOSIZE);
    cv::putText(img0, "Result of automatically detecting the Go board.", 
                cvPoint(20,20), 1, 0.8, cv::Scalar(64, 64, 255), 1, CV_AA);
    showImage();
    cv::waitKey(0);
    cv::destroyWindow(windowName);

    asked_for_board_contour = true;
}

// Map pixel coordinates (intersection points) to board coordinates
// Begins at the left of the lowermost line and gradually moves the lines from left to right upwards.
//
// Preconditions:
// - The number of intersection points must match board_size * board_size!
// - Each line (or column if you want) must have exactly the same number of intersection points as the board_size value
// - It is required that the intersection points on a specific line all have almost equal y-values!
//   Thats the decision criteria for selecting the points on a given line.
std::map<cv::Point2f, SgPoint, lesserPoint2f> getBoardCoordMapFor(std::vector<cv::Point2f> intersectionPoints, int board_size) {
    const auto& num_lines = board_size;

    // Sort points by their descending y-values to begin at the lowermost point
    std::sort(std::begin(intersectionPoints), std::end(intersectionPoints), [](const cv::Point2f& left, const cv::Point2f& right) { return left.y > right.y; });

    // Walk through each line from left to right and map a board coordinate to each point,
    // iteratively looks at the next batch (num_lines points) of intersection points
    std::map<cv::Point2f, SgPoint, lesserPoint2f> to_board_coordinates;
    int line_idx = 1;
    auto begin_iter = begin(intersectionPoints);
    for (auto i = 0; i < num_lines; ++i, begin_iter += num_lines) {
        // Get the next num_lines points (these will be the points on the i-th line from the bottom)
        decltype(intersectionPoints) points_on_this_line;
        std::copy( begin_iter,
                   begin_iter+num_lines,
                   std::back_inserter(points_on_this_line));

        // Sort by ascending x values of the points
        std::sort(std::begin(points_on_this_line), std::end(points_on_this_line), [](const cv::Point2f& left, const cv::Point2f& right) { return left.x < right.x; });

        // Add an entry for each point on this line to the map
        int column_idx = 1;
        for (const auto& pt : points_on_this_line) {
            to_board_coordinates[pt] = SgPointUtil::Pt(column_idx++, line_idx);
        }

        ++line_idx;
    }

    return to_board_coordinates;
}

/**
 * @returns     true, if the user marked the board, and lines as well as stones could be found
 *              false, if the board wasn't marked before or if any of the operations fail (detecting stones, finding lines, etc.)
 */
bool scanner_main(const cv::Mat& camera_frame, GoSetup& setup, int& board_size)
{
    // TODO: convert the warped image just once to greyscale! 

    img0 = camera_frame; //cv::imread("go_bilder/01.jpg");

    imagewidth  = img0.cols;
    imageheight = img0.rows;

    // only process the image if the user  selected the board with "ask_for_board_contour" or "do_auto_board_detection" once.
    // this is triggered through the GUI (and the Scanners selectBoardManually() and selectBoardAutomatically() methods)
    if (!asked_for_board_contour) {
        return false;
    }

    //imshow("Camera Input", img0);
    
    cv::Mat warpedImg = warpImage(img0,
        cv::Point2f(boardCornerX[0], boardCornerY[0]),
        cv::Point2f(boardCornerX[1], boardCornerY[1]),
        cv::Point2f(boardCornerX[3], boardCornerY[3]),
        cv::Point2f(boardCornerX[2], boardCornerY[2]));

    if(showall == true)
        cv::imshow("Warped Image", warpedImg);

    cv::Mat srcWarpedImg = warpedImg.clone();
    cv::Mat paintedWarpedImg = warpedImg.clone();
    cv::vector<cv::Point2f> intersectionPoints;

    if (debug == true)
    {
        Thresh = warpedImg.clone();
        cv::cvtColor(Thresh, Thresh, CV_RGB2GRAY);

        cv::namedWindow( window_name_thre, CV_WINDOW_AUTOSIZE );

        cv::createTrackbar( trackbar_type,
                  window_name_thre, &threshold_type,
                  max_type, Threshold_Debug );


        cv::createTrackbar( trackbar_value,
                  window_name_thre, &threshold_value,
                  max_value, Threshold_Debug );

        cv::waitKey();
    }

    else
    {
        bool intersectionResult = getBoardIntersections(warpedImg, 255, intersectionPoints, paintedWarpedImg);
    }

    // @todo: assert that the number of intersection points are a quadratic number


    // Calc the minimum distance between the first intersection point to all others
    // The minimum distance is approximately the diameter of a stone
    vector<float> distances;
    const auto& ref_point_min = *begin(intersectionPoints);
    for (const auto& point : intersectionPoints) {
        if (point != ref_point_min)
            distances.push_back(cv::norm(point - ref_point_min));
    }
    auto approx_stone_diameter = *std::min_element(begin(distances), end(distances));

    // Extract the board size
    // Board dimensions are quadratic, meaning width and height are the same so the sqrt(of the number of intersections) 
    // is the board size if it is a perfect square
    board_size = (int) floor( sqrt((double) intersectionPoints.size()) + 0.5 ); // The .5 is needed to round to the nearest integer
    if (board_size*board_size != intersectionPoints.size()) {
        // Got a false number of intersectionPoints
        // Stop the processing here
        return false;
    }

    std::cerr << "Board size: " << board_size << std::endl;

    // get map from intersection points to board coordinates (SgPoint)
    auto to_board_coords = getBoardCoordMapFor(intersectionPoints, board_size);

    // detect the stones!
    SgPointSet all_stones;
    detectAllStones(srcWarpedImg, intersectionPoints, to_board_coords, all_stones, approx_stone_diameter, paintedWarpedImg);

    SgPointSet black_stones;
    detectBlackStones(srcWarpedImg, intersectionPoints, to_board_coords, black_stones, paintedWarpedImg);
    setup.m_stones[SG_BLACK] = black_stones;
    setup.m_stones[SG_WHITE] = all_stones - black_stones;

    cv::imshow("Detected Stones and Intersections", paintedWarpedImg);

    return true;
}

}
/*
 * image adjust.  Automatic image normalization.
 * Copyright (C) 2010 Joel Granados <joel.granados@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "IA_Square.h"
#include <iostream>
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <stdio.h>

//using namespace cv;

IA_Square::IA_Square ( Point2f *p[4], const Mat *img )
{
  /* Initialize the array that will hold the bits. */
  rgb[0]=rgb[1]=rgb[2]=0;

  /* initialize the sub_image */
  Rect t_rect = Rect( /* helper rectangle (x, y, width, height) */
    X_MIN (p[0], p[1], p[2], p[3]),
    Y_MIN (p[0], p[1], p[2], p[3]),
    O_S_WIDTH(p[0],p[1],p[2],p[3]), O_S_HEIGHT(p[0],p[1],p[2],p[3])
  );
  hsv_subimg = Mat( *img, t_rect );

  /* We separate hsv into its different dimensions */
  vector<Mat> tmp_dim;
  split( hsv_subimg, tmp_dim );
  h_subimg = &tmp_dim[0];
  s_subimg = &tmp_dim[1];
  v_subimg = &tmp_dim[2];

  /* Initialize the square struct */
  for ( int i = 0 ; i <= 3 ; i++ )
  {
    sqr.ps[i] = new ia_square_point;
    sqr.ls[i] = new ia_square_line;
  }

  /* Interconnect lines and points.  None point to anything */
  for ( int i = 0 ; i <= 3 ; i++ )
  {
    /* Create a double linked list of points */
    sqr.ps[i]->padjs[0] = sqr.ps[ (i+1)%4 ];
    sqr.ps[ (i+1)%4 ]->padjs[1] = sqr.ps[i];

    /* Create a double linked list of lines */
    sqr.ls[i]->ladjs[0] = sqr.ls[ (i+1)%4 ];
    sqr.ls[ (i+1)%4 ]->ladjs[1] = sqr.ls[i];

    /* link each line with it's respective point */
    sqr.ls[i]->lps[0] = sqr.ps[i];
    sqr.ls[i]->lps[1] = sqr.ps[ (i+1)%4 ];

    /* link each point with it's respective line */
    sqr.ps[i]->pls[0] = sqr.ls[i];
    sqr.ps[ (i+1)%4 ]->pls[1] = sqr.ls[i];
  }

  /* We fill in the point pointers */
  for ( int i = 0 ; i <= 3 ; i++ ) /* dim 0 and 1 will never be negative */
    sqr.ps[i]->pref = Point2f ( floor(p[i]->x-t_rect.x),
                                floor(p[i]->y-t_rect.y) );

  /* We fill in the square lines */
  for ( int i = 0 ; i <= 3 ; i++ )
    sqr.ls[i]->lref =  new IA_Line ( &sqr.ls[i]->lps[0]->pref,
                                     &sqr.ls[i]->lps[1]->pref );

  calculate_rgb();
}

void
IA_Square::calculate_rgb ()
{
  struct ia_square_line *line1, *line2;
  int col1, col2;
  float ca_angle = 0;

  /* We analyze all the rows in the image.  The next for loop contains two
   * steps: 1. We select the lines that intersec the row that is being analized,
   * and 2. we traverse that row from left line to right line and do a
   * cumulative average.*/
  line1 = sqr.ls[0]; /* select random lines to begin */
  line2 = sqr.ls[1];
  for ( unsigned int row = 0 ; row <= h_subimg->rows ; row++ )
  {
    /* Step 1: We don't change the lines if row intersects them.  If row
     * does not intersect at leaset one, we use row_between_lines to find new
     * lines. 'row' here can be seen as a horizontal line.*/
    if ( ! row_between_lines ( row, line1, line2 ) )
    {
      bool found = false;
      for ( int i = 0 ; i <= 3 ; i++ )
        if ( row_between_lines ( row, sqr.ls[i], sqr.ls[(i+1)%4] ) )
        {
          found = true;
          line1 = sqr.ls[i];
          line2 = sqr.ls[(i+1)%4];
          break;
        }
      if ( ! found )
      {
        if ( row_between_lines ( row, sqr.ls[0], sqr.ls[2] ) )
        {
          line1 = sqr.ls[0];
          line2 = sqr.ls[2];
        } else {
          line1 = sqr.ls[1];
          line2 = sqr.ls[3];
        }
      }
    }

    /* At this point we are sure that line1 and line2 intersect.  We now
     * calculate col1 (left) and col2 (right).*/
    col1=min(line1->lref->resolve_width(row), line2->lref->resolve_width(row));
    col2=max(line1->lref->resolve_width(row), line2->lref->resolve_width(row));

    /* Step 2: We traverse all of 'row' from col1 (left) to col2 (right) and do
     * a cumulative average*/
    for ( int i = 0 ; col1 + i < col2 ; i++ )
    {
      ca_angle = ( *(h_subimg->data + h_subimg->cols * row + col1 + i)
                   + (i*ca_angle) )/(i+1);
      std::cout << "|" <<(float)*(h_subimg->data + h_subimg->cols * row + col1 + i);
    }
      //ca_angle = ((*h_subimg).at<float>(col1+1, row) + (i*ca_angle))/(i+1);
  }
  std::cout <<  "<=" << ca_angle << "|" << endl;

  /*
   * We calculate rgb array from ca_angle with the folloing table.
   *  red                 -> (348.33,380] || [0,31.66]
   *  yellow (red-green)  -> (31.66,95]
   *  green               -> (95,158.33]
   *  cyan (green-blue)   -> (158.33,221.66]
   *  blue                -> (221.66,285]
   *  magenta (red-blue)  -> (285,348.33]
   *  This is dependant on RBGtoHSV transformation in IAChessboardImage.
   */
  if ( ca_angle > 31.66 && ca_angle <= 95 ) {
    rgb[0] = 1;
    rgb[1] = 1;
  } else if ( ca_angle > 95 && ca_angle <= 158.33 ) {
    rgb[1] = 1;
  } else if ( ca_angle > 158.33 && ca_angle <= 221.66 ) {
    rgb[1] = 1;
    rgb[2] = 1;
  } else if ( ca_angle > 221.66 && ca_angle <= 285 ) {
    rgb[2] = 1;
  } else if ( ca_angle > 285 && ca_angle <= 348.33 ) {
    rgb[0] = 1;
    rgb[2] = 1;
  } else if ( (ca_angle > 348.33 && ca_angle <= 380)
              || (ca_angle >= 0 && ca_angle <= 31.66) ){
    rgb[0] = 1;
  } else
    ;/* It should not get here */
}

inline bool
IA_Square::row_between_lines ( const unsigned int row,
                               const struct ia_square_line *line1,
                               const struct ia_square_line *line2 )
{
  /* The row (horizontal line) is between the lines only if the maximum of the
   * lines starting point is less than row and if the minimum of the lines
   * ending points is greater than row.*/
  if ( max( min(line1->lps[0]->pref.y,line1->lps[1]->pref.y),
            min(line2->lps[0]->pref.y,line2->lps[1]->pref.y) ) <= row
      && min ( max(line1->lps[0]->pref.y,line1->lps[1]->pref.y),
               max(line2->lps[0]->pref.y,line2->lps[1]->pref.y) ) >= row )
    return true;
  return false;
}

int
IA_Square::get_red_value ()
{
  return rgb[0];
}

int
IA_Square::get_green_value ()
{
  return rgb[1];
}

int
IA_Square::get_blue_value ()
{
  return rgb[2];
}

int*
IA_Square::get_values ()
{
  return rgb;
}

IA_Line::IA_Line ( Point2f *p1, Point2f *p2 )
{
  this->p1 = p1;
  this->p2 = p2;

  horizontal = vertical = false;
  if ( p2->y - p1->y == 0 ) /* We have a horizontal line */
    horizontal = true;

  else if ( p2->x - p1->x == 0 ) /* We have a vertical line */
    vertical = true;

  else
    /* We have a "normal" line. */
    slope = ( p2->y - p1->y )/( p2->x - p1->x );
}

int
IA_Line::resolve_width ( int height )
{
  /* it's an error to ask for width of a horizontal line */
  if ( horizontal )
    return -1;

  else if ( vertical )
    return p1->x; /* == to p2->x */

  else
    return floor ( (height - p1->y)/slope + p1->x );
}

int
IA_Line::resolve_height ( int width )
{
  if ( horizontal )
    return p1->y; /*== to p2->y*/

  /* it's an error to ask for height of a vertical line */
  else if ( vertical )
    return -1;

  else
    return floor ( slope * (width - p1->x) + p1->y );
}


IA_ChessboardImage::IA_ChessboardImage ( string &image, Size &boardSize )
{
  Mat a_image = Mat::zeros(1,1,CV_64F); //adjusted image
  Mat hsv_img; //temp image
  vector<Point2f> pointbuf;

  /* get next image*/
  a_image = imread ( image );

  try
  {
    /* Initialize gray image here so the scope takes care of it for us */
    Mat g_img; //temp gray image
    /* transform to grayscale */
    cvtColor ( a_image, g_img, CV_BGR2GRAY );

    /* find the chessboard points in the image and put them in pointbuf.*/
    if ( !findChessboardCorners(g_img, boardSize, (pointbuf),
                                CV_CALIB_CB_ADAPTIVE_THRESH) )
      has_chessboard = false;

    else
      /* improve the found corners' coordinate accuracy */
      cornerSubPix ( g_img, (pointbuf), Size(11,11), Size(-1,-1),
                     TermCriteria(CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 30, 0.1) );
  }catch (cv::Exception){has_chessboard = false;}

  if (has_chessboard)
  {
    cvtColor ( a_image, hsv_img, CV_BGR2HSV_FULL );

    bool isBlack = true;
    for ( int r = 0 ; r < boardSize.height-1 ; r++ )
      for ( int c = 0 ; c < boardSize.width-1 ; c++ )
      {
        Point2f *ordered_points[4];
        ordered_points[0] = &pointbuf[ (r*boardSize.width)+c ];
        ordered_points[1] = &pointbuf[ (r*boardSize.width)+c+1 ];
        ordered_points[2] = &pointbuf[ (r*boardSize.width)+boardSize.width+c+1 ];
        ordered_points[3] = &pointbuf[ (r*boardSize.width)+boardSize.width+c ];

        if ( !isBlack )
        {
          squares.push_back( IA_Square( ordered_points, &hsv_img ) );
          isBlack = !isBlack;
        }
        else
          isBlack = !isBlack;
      }
  }
}

void
IA_ChessboardImage::debug_print ()
{
  std::cout << endl << "Printing red\t";
  for ( int i = 0; i < squares.size() ; i++ )
    std::cout << squares[i].get_red_value();

  std::cout << endl << "Printing green\t";
  for ( int i = 0; i < squares.size() ; i++ )
    std::cout << squares[i].get_green_value();

  std::cout << endl << "Printing blue\t";
  for ( int i = 0; i < squares.size() ; i++ )
    std::cout << squares[i].get_blue_value();

  std::cout << endl;
}

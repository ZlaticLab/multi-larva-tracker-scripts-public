//  MWT_behavior_detection.cpp : Defines the exported functions for the DLL application.

#include "stdafx.h"
#include <iostream>
#include <map>
#include <sstream>
#include <utility>
#include <set>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include "opencv2/imgproc/imgproc.hpp"
#include <stack>
#include <ctime>
#include <cstdlib>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <iterator>
#include <numeric>
#include <cmath>
#include <random>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


// Runtime measurement
std::stack<clock_t> tictoc_stack;
double tictocdiff;

void tic() {
	tictoc_stack.push(clock());
}

void toc() {
	tictocdiff = ((double)(clock() - tictoc_stack.top())) / CLOCKS_PER_SEC;
	tictoc_stack.pop();
}

// General variables
int larvaeToTrack = 16; // number of larvae to stimulate
int nBuffers = 5; // depth of buffer in which past feature information is stored
int frame = 0; // initialize image frame index within experiment
int next_ind = 0; // variable tracking location within buffer
int prev_ind; // variable storing previous location within buffer
double gamma_spine = 0; // smoothing parameter for spine
int nSpine; // number of spine points
int velocity_step = 4; // number of frames over which velocities are calculated
int nContourReconstr = 100; // number of points in Fourier reconstructed contour
int nFourier = 7; // number of Fourier coefficients
enum FourCoef { AX = 0, BX = 1, AY = 2, BY = 3 }; // redefines indices for Fourier coefficient vectors
int stopExperiment = 0;

 // Variables needed for galvo control
int n_larvae_per_galvo = 4; // needs to be a multiple of 4
							// int n_cycles_per_larva = 2; // number of galvo frames which are spent at each larva

// Variables needed for yoked control
int yokedFileLength; // number of larvae in yoked file
std::vector<int> yokedLarvaIndices; // shuffled indices for random yoked control
std::vector<std::vector<double> > yokedFileContent;
bool yokedMode = false;

std::vector<int> larvaeIDsPast(1); // needed to transfer buffer data between frames

std::vector<double> time_stamp(nBuffers); // Time stamp [s]
std::vector<double> time_stamp_last_ball; // Time stamp of last ball [s]
std::vector<double> time_stamp_last_roll; // Time stamp of last roll [s]

std::vector<std::vector<cv::Point2f> > spine_prev; // previous spine data used for smoothing spine
std::vector<std::vector<cv::Point2f> > spine_smooth; // smooth spine

std::string savingPath = ""; // initialize path string for saving data
std::vector<std::string> stimLabelsVector; // stores labels for stimulation parameters as defined by the user

// Initialize landmark points
std::vector<std::vector<cv::Point2f> > head(nBuffers); // Head location [mm]
std::vector<std::vector<cv::Point2f> > tail(nBuffers); // Tail location [mm]
std::vector<std::vector<cv::Point2f> > centroid(nBuffers); // Centroid location [mm]
std::vector<std::vector<cv::Point2f> > neck(nBuffers); // Neck location [mm]
std::vector<std::vector<cv::Point2f> > neck_top(nBuffers); // Neck top location [mm]
std::vector<std::vector<cv::Point2f> > neck_down(nBuffers); // Neck down location [mm]

// Initialize features
std::vector<std::vector<double> > larva_arc_ratio(nBuffers); // Ratio of contour arc length and convex hull arc length
std::vector<std::vector<double> > larva_area_ratio(nBuffers); // Ratio of contour area and convex hull area
std::vector<std::vector<double> > skeleton_length(nBuffers); // Skeleton length [mm]
std::vector<std::vector<double> > perimeter(nBuffers); // Perimeter of contour [mm]
std::vector<std::vector<double> > angle_upper_lower(nBuffers); // Angle between head and body
std::vector<std::vector<cv::Point2f> > direction_vector(nBuffers); // Neck minus neck_down (normalized)
std::vector<std::vector<cv::Point2f> > direction_head_vector(nBuffers); // Head minus neck_top (normalized)
std::vector<std::vector<cv::Point2f> > direction_tail_vector(nBuffers); // Neck_down minus tail (normalized)
std::vector<std::vector<cv::Point2f> > head_velocity(nBuffers); // Head velocity by x and y components [mm/s]
std::vector<std::vector<double> > head_speed(nBuffers); // Head velocity [mm/s]
std::vector<std::vector<cv::Point2f> > tail_velocity(nBuffers); // Tail velocity by x and y components [mm/s]
std::vector<std::vector<double> > tail_speed(nBuffers); // Tail velocity [mm/s]
std::vector<std::vector<cv::Point2f> > neck_velocity(nBuffers); // Neck velocity by x and y components [mm/s]
std::vector<std::vector<double> > neck_speed(nBuffers); // Neck velocity [mm/s]
std::vector<std::vector<cv::Point2f> > neck_top_velocity(nBuffers); // Neck_top velocity by x and y components [mm/s]
std::vector<std::vector<double> > neck_top_speed(nBuffers); // Neck_top velocity [mm/s]
std::vector<std::vector<cv::Point2f> > neck_down_velocity(nBuffers); // Neck_down velocity by x and y components [mm/s]
std::vector<std::vector<double> > neck_down_speed(nBuffers); // Neck_down velocity [mm/s]
std::vector<std::vector<double> > crab_speed(nBuffers); // Sideways velocity [mm/s]
std::vector<std::vector<double> > v_norm(nBuffers); // Mean velocity of landmark points [mm/s]
std::vector<std::vector<double> > v_centroid(nBuffers); // Velocity of centroid [mm/s]
std::vector<std::vector<double> > speed_reduced(nBuffers); // Relative contribution of the velocity of neck_top to v_norm [mm/s]
std::vector<std::vector<double> > s(nBuffers); // Nematic S
std::vector<std::vector<double> > eig_reduced(nBuffers); // Normalized contour eigenvalue difference
std::vector<std::vector<double> > asymmetry(nBuffers); // Asymmetric measure of the angle between head and body
std::vector<std::vector<double> > damped_distance(nBuffers); // Damped measure of the distance traveled over time [mm]
std::vector<std::vector<double> > parallel_speed(nBuffers); // Cosine of direction of neck movement
std::vector<std::vector<double> > parallel_speed_tail(nBuffers); // Cosine of direction of tail movement
std::vector<std::vector<double> > parallel_speed_tail_raw(nBuffers); // Velocity component of tail movement that is parallel to direction_tail_vector [mm/s]

// Temporally smoothed versions of the above features
std::vector<std::vector<double> > v_norm_filtered(nBuffers);
std::vector<std::vector<double> > v_centroid_filtered(nBuffers);
std::vector<std::vector<double> > crab_speed_filtered(nBuffers);
std::vector<std::vector<double> > angle_upper_lower_filtered(nBuffers);
std::vector<std::vector<double> > s_filtered(nBuffers);
std::vector<std::vector<double> > asymmetry_filtered(nBuffers);
std::vector<std::vector<double> > eig_reduced_filtered(nBuffers);
std::vector<std::vector<double> > skeleton_length_filtered(nBuffers);
std::vector<std::vector<double> > perimeter_filtered(nBuffers);
std::vector<std::vector<double> > damped_distance_filtered(nBuffers);
std::vector<std::vector<double> > speed_reduced_filtered(nBuffers);
std::vector<std::vector<double> > parallel_speed_filtered(nBuffers);
std::vector<std::vector<double> > parallel_speed_tail_filtered(nBuffers);
std::vector<std::vector<double> > parallel_speed_tail_raw_filtered(nBuffers);
std::vector<std::vector<cv::Point2f> > neck_velocity_filtered(nBuffers);
std::vector<std::vector<cv::Point2f> > tail_velocity_filtered(nBuffers);
std::vector<std::vector<cv::Point2f> > head_velocity_filtered(nBuffers);
std::vector<std::vector<cv::Point2f> > neck_top_velocity_filtered(nBuffers);
std::vector<std::vector<cv::Point2f> > neck_down_velocity_filtered(nBuffers);
std::vector<std::vector<cv::Point2f> > direction_vector_filtered(nBuffers);
std::vector<std::vector<cv::Point2f> > direction_tail_vector_filtered(nBuffers);
std::vector<std::vector<cv::Point2f> > direction_head_vector_filtered(nBuffers);
std::vector<std::vector<double> > v_norm_long_time(nBuffers); // smoothed over a longer time window
std::vector<std::vector<double> > v_centroid_long_time(nBuffers); // smoothed over a longer time window

// Variables needed for calculating derivative of above features
std::vector<std::vector<double> > v_norm1(nBuffers);
std::vector<std::vector<double> > v_norm2(nBuffers);
std::vector<std::vector<double> > crab_speed1(nBuffers);
std::vector<std::vector<double> > crab_speed2(nBuffers);
std::vector<std::vector<double> > angle_upper_lower1(nBuffers);
std::vector<std::vector<double> > angle_upper_lower2(nBuffers);
std::vector<std::vector<double> > s1(nBuffers);
std::vector<std::vector<double> > s2(nBuffers);
std::vector<std::vector<double> > asymmetry1(nBuffers);
std::vector<std::vector<double> > asymmetry2(nBuffers);
std::vector<std::vector<double> > eig_reduced1(nBuffers);
std::vector<std::vector<double> > eig_reduced2(nBuffers);
std::vector<std::vector<double> > skeleton_length1(nBuffers);
std::vector<std::vector<double> > skeleton_length2(nBuffers);
std::vector<std::vector<double> > perimeter1(nBuffers);
std::vector<std::vector<double> > perimeter2(nBuffers);
std::vector<std::vector<double> > damped_distance1(nBuffers);
std::vector<std::vector<double> > damped_distance2(nBuffers);
std::vector<std::vector<double> > speed_reduced1(nBuffers);
std::vector<std::vector<double> > speed_reduced2(nBuffers);
std::vector<std::vector<double> > parallel_speed1(nBuffers);
std::vector<std::vector<double> > parallel_speed2(nBuffers);
std::vector<std::vector<double> > parallel_speed_tail1(nBuffers);
std::vector<std::vector<double> > parallel_speed_tail2(nBuffers);
std::vector<std::vector<double> > v_norm_convolved_squared(nBuffers);
std::vector<std::vector<double> > crab_speed_convolved_squared(nBuffers);
std::vector<std::vector<double> > angle_upper_lower_convolved_squared(nBuffers);
std::vector<std::vector<double> > s_convolved_squared(nBuffers);
std::vector<std::vector<double> > asymmetry_convolved_squared(nBuffers);
std::vector<std::vector<double> > eig_reduced_convolved_squared(nBuffers);
std::vector<std::vector<double> > skeleton_length_convolved_squared(nBuffers);
std::vector<std::vector<double> > perimeter_convolved_squared(nBuffers);
std::vector<std::vector<double> > damped_distance_convolved_squared(nBuffers);
std::vector<std::vector<double> > speed_reduced_convolved_squared(nBuffers);
std::vector<std::vector<double> > parallel_speed_convolved_squared(nBuffers);
std::vector<std::vector<double> > parallel_speed_tail_convolved_squared(nBuffers);

// Initialise store for top larvae indices
std::vector<int> topLarvaeIndices(larvaeToTrack);

// Variables needed for an improved head tail detection
std::vector<int> votes_correct;
std::vector<int> votes_flipped;
std::vector<bool> flippedHeadTail;
std::vector<cv::Point2f> master_head;
std::vector<cv::Point2f> master_tail;
std::vector<bool> flipLarvaeID;

// Variables needed to smooth behaviors
std::vector<std::vector<double> > d_roll(nBuffers);
std::vector<std::vector<double> > d_bend(nBuffers);
std::vector<std::vector<double> > d_roll_smooth(nBuffers);
std::vector<std::vector<double> > d_bend_smooth(nBuffers);

// Behavior classifiers
std::vector<std::vector<bool> > bend(nBuffers);
std::vector<std::vector<bool> > ball(nBuffers);
std::vector<std::vector<bool> > left(nBuffers);
std::vector<std::vector<bool> > right(nBuffers);
std::vector<std::vector<bool> > back(nBuffers);
std::vector<std::vector<bool> > roll(nBuffers);
std::vector<std::vector<bool> > forward(nBuffers);
std::vector<std::vector<bool> > forward_peristaltic(nBuffers);
std::vector<std::vector<bool> > weird(nBuffers);
std::vector<std::vector<bool> > roll_weird(nBuffers);

// Temporally smoothed behavior classifiers
std::vector<std::vector<bool> > roll_smooth(nBuffers);
//std::vector<std::vector<bool> > left_smooth(nBuffers);
//std::vector<std::vector<bool> > right_smooth(nBuffers);
std::vector<std::vector<bool> > bend_smooth(nBuffers);

// Variables needed for defining stimulation protocols
std::vector<bool> stimulating; // for random stimulation protocols
std::vector<bool> last_bend_direction; // stores direction of previous bend

// Variables needed to update buffers of valid larvae
std::vector<int> larvaeIDsPastIndex; // stores the location of a larva in the previous buffers
std::vector<bool> existentInPast; // keeps track of whether a larva was previously detected
std::vector<bool> existentAndValidInPast; // keeps track of whether a larva was previously detected and has been valid in the past
std::vector<bool> validInPresentOrPast; // keeps track of whether a larva has been valid in the present or past

// Variables needed to generate stimulus output
std::vector<double> dmdIntensities;
std::vector<double> irIntensities;
std::vector<int> outputVector;
std::string outputVectorString;

// REVIEW
std::pair<double, double> mid; // stores the center of polygon in convex_hull (It is made global becuase it is used in compare function)
double time_stamp_start; // time stamp at begin of experiment

// Initialize file streams
std::ofstream behaviorFile;
std::ofstream configFile;
std::ofstream stimulationFile;
std::ofstream hullFile;
std::ofstream contourFile;
std::ofstream spineFile;
std::ofstream galvoFile;

// Call main function
_declspec(dllexport) void behaviorClassifier(char* dateTimePtr, int dateTimeLength, char* genotype1Ptr, int genotype1Length, char* genotype2Ptr, int genotype2Length,
	char* stimulationModePtr, int stimulationModeLength, char* rigNamePtr, int rigNameLength, int experimentStart, int experimentStop,
	int frameNum, double timeStamp, int numLarvae, int* larvaeIDsPtr, int* nContourPtr, int* xContourPtr, int* yContourPtr,
	int* xSpinePtr, int* ySpinePtr, int* larvaeValidPtr, int lightIntensity, char* savingPathPtr, int savingPathLength, char* yokedFileNamePtr, int yokedFileNameLength, int yokedFileEnable, double gamma_loc,
	double PixelResolution, int ImageHeight, int ImageWidth, int PixelIntensityLow, int PixelIntensityHigh, int BoxSideLow, int BoxSideHigh,
	int BoxWLLow, int BoxWLHigh, int AreaLow, int AreaHigh, int nSpineLoc, int BGSubTH, int minContourPoints, double stim1,
	double stim2, double stim3, double stim4, double stim5, double stim6, double stim7, double stim8, double stim9, double stim10,
	char* stimLabelsPtr, int stimLabelsLength, int* flipLarvaeIDPtr, int flipLarvaeIDLength, int laserIntensity, int* outData, int* voteReset, int* galvoParameters);


// Fourier decomposition of contour
std::vector<std::vector<double> > fourierDecompose(std::vector<double> xContourTemp, std::vector<double> yContourTemp, int nContourTemp)
{
	// initialize Fourier coefficients
	std::vector<std::vector<double> > fourierCoeffs;
	fourierCoeffs.resize(nFourier);

	for (int i = 0; i<nFourier; ++i) {
		fourierCoeffs[i].resize(4);

		// 0:AX,1:BX,2:AY,3:BY - defined in enum FourCoef
		for (int j = 0; j<4; ++j)
		{
			fourierCoeffs[i][j] = 0.0;
		}

		// dot products
		for (int j = 0; j<nContourTemp; ++j)
		{
			fourierCoeffs[i][AX] += double(xContourTemp[j]) * cos(double(i) * M_PI * double(j) * 2.0 / double(nContourTemp));
			fourierCoeffs[i][BX] += double(xContourTemp[j]) * sin(double(i) * M_PI * double(j) * 2.0 / double(nContourTemp));
			fourierCoeffs[i][AY] += double(yContourTemp[j]) * cos(double(i) * M_PI * double(j) * 2.0 / double(nContourTemp));
			fourierCoeffs[i][BY] += double(yContourTemp[j]) * sin(double(i) * M_PI * double(j) * 2.0 / double(nContourTemp));
		}

		// normalize
		for (int j = 0; j<4; ++j)
		{
			fourierCoeffs[i][j] *= 2.0 / double(nContourTemp);
		}
	}

	fourierCoeffs[0][AX] /= 2;
	fourierCoeffs[0][AY] /= 2;

	return fourierCoeffs;
}


// Fourier reconstruction of contours
void fourierReconstruct(std::vector<std::vector<std::vector<double> > > fourierCoeffs, std::vector<std::vector<cv::Point2f> > & contour_loc)
{
	// iterate over all larvae
	for (int ind = 0; ind < contour_loc.size(); ++ind) {
		contour_loc[ind].resize(nContourReconstr);

		// theta goes from -PI to PI in nContourReconstr steps
		double theta = -M_PI;
		double theta_step = 2.0*M_PI / double(nContourReconstr);

		for (int i = 0; i<nContourReconstr; ++i)
		{
			contour_loc[ind][i] = cv::Point2f(0, 0); // initialize points
		}

		for (int k = 0; k<nFourier; ++k)
		{
			theta = -M_PI;
			for (int i = 0; i<nContourReconstr; ++i)
			{
				contour_loc[ind][i].x += fourierCoeffs[ind][k][AX] * cos(double(k) * theta) + fourierCoeffs[ind][k][BX] * sin(double(k) * theta);
				contour_loc[ind][i].y += fourierCoeffs[ind][k][AY] * cos(double(k) * theta) + fourierCoeffs[ind][k][BY] * sin(double(k) * theta);
				theta += theta_step;
			}
		}
	}
}


// Write .behavior file
void writeBehaviorFile(int experimentStart, int experimentStop, std::string dateTime, std::vector<int> larvaeIDs, std::vector<bool> larvaeValid)
{
	// get index in buffer
	int next_ind_loc;
	if (next_ind == 0)
	{
		next_ind_loc = nBuffers;
	}
	else
	{
		next_ind_loc = next_ind;
	}

	// get time interval between previous and current frame
	double dt_eff = time_stamp[next_ind] - time_stamp[next_ind_loc - 1];

	// for all larvae
	for (int ind = 0; ind<larvaeIDs.size(); ++ind) {

		behaviorFile << dateTime << " "
			<< larvaeIDs[ind] << " "
			<< time_stamp[next_ind] << " "
			<< frame << " "
			<< larvaeIDsPastIndex[ind] << " "
			<< head[next_ind][ind].x << " "
			<< head[next_ind][ind].y << " "
			<< tail[next_ind][ind].x << " "
			<< tail[next_ind][ind].y << " "
			<< centroid[next_ind][ind].x << " "
			<< centroid[next_ind][ind].y << " "
			<< neck[next_ind][ind].x << " "
			<< neck[next_ind][ind].y << " "
			<< neck_top[next_ind][ind].x << " "
			<< neck_top[next_ind][ind].y << " "
			<< neck_down[next_ind][ind].x << " "
			<< neck_down[next_ind][ind].y << " "
			<< larva_arc_ratio[next_ind][ind] << " "
			<< larva_area_ratio[next_ind][ind] << " "
			<< skeleton_length[next_ind][ind] << " "
			<< perimeter[next_ind][ind] << " "
			<< angle_upper_lower[next_ind][ind] << " "
			<< direction_vector[next_ind][ind].x << " "
			<< direction_vector[next_ind][ind].y << " "
			<< direction_head_vector[next_ind][ind].x << " "
			<< direction_head_vector[next_ind][ind].y << " "
			<< direction_tail_vector[next_ind][ind].x << " "
			<< direction_tail_vector[next_ind][ind].y << " "
			<< head_velocity[next_ind][ind].x << " "
			<< head_velocity[next_ind][ind].y << " "
			<< head_speed[next_ind][ind] << " "
			<< tail_velocity[next_ind][ind].x << " "
			<< tail_velocity[next_ind][ind].y << " "
			<< tail_speed[next_ind][ind] << " "
			<< neck_velocity[next_ind][ind].x << " "
			<< neck_velocity[next_ind][ind].y << " "
			<< neck_speed[next_ind][ind] << " "
			<< neck_top_velocity[next_ind][ind].x << " "
			<< neck_top_velocity[next_ind][ind].y << " "
			<< neck_top_speed[next_ind][ind] << " "
			<< neck_down_velocity[next_ind][ind].x << " "
			<< neck_down_velocity[next_ind][ind].y << " "
			<< neck_down_speed[next_ind][ind] << " "
			<< crab_speed[next_ind][ind] << " "
			<< v_norm[next_ind][ind] << " "
			<< v_centroid[next_ind][ind] << " "
			<< speed_reduced[next_ind][ind] << " "
			<< s[next_ind][ind] << " "
			<< eig_reduced[next_ind][ind] << " "
			<< asymmetry[next_ind][ind] << " "
			<< damped_distance[next_ind][ind] << " "
			<< parallel_speed[next_ind][ind] << " "
			<< parallel_speed_tail[next_ind][ind] << " "
			<< parallel_speed_tail_raw[next_ind][ind] << " "
			<< v_norm_filtered[next_ind][ind] << " "
			<< v_centroid_filtered[next_ind][ind] << " "
			<< crab_speed_filtered[next_ind][ind] << " "
			<< angle_upper_lower_filtered[next_ind][ind] << " "
			<< s_filtered[next_ind][ind] << " "
			<< asymmetry_filtered[next_ind][ind] << " "
			<< eig_reduced_filtered[next_ind][ind] << " "
			<< skeleton_length_filtered[next_ind][ind] << " "
			<< perimeter_filtered[next_ind][ind] << " "
			<< damped_distance_filtered[next_ind][ind] << " "
			<< speed_reduced_filtered[next_ind][ind] << " "
			<< parallel_speed_filtered[next_ind][ind] << " "
			<< parallel_speed_tail_filtered[next_ind][ind] << " "
			<< parallel_speed_tail_raw_filtered[next_ind][ind] << " "
			<< neck_velocity_filtered[next_ind][ind].x << " "
			<< neck_velocity_filtered[next_ind][ind].y << " "
			<< tail_velocity_filtered[next_ind][ind].x << " "
			<< tail_velocity_filtered[next_ind][ind].y << " "
			<< head_velocity_filtered[next_ind][ind].x << " "
			<< head_velocity_filtered[next_ind][ind].y << " "
			<< neck_top_velocity_filtered[next_ind][ind].x << " "
			<< neck_top_velocity_filtered[next_ind][ind].y << " "
			<< neck_down_velocity_filtered[next_ind][ind].x << " "
			<< neck_down_velocity_filtered[next_ind][ind].y << " "
			<< direction_vector_filtered[next_ind][ind].x << " "
			<< direction_vector_filtered[next_ind][ind].y << " "
			<< direction_tail_vector_filtered[next_ind][ind].x << " "
			<< direction_tail_vector_filtered[next_ind][ind].y << " "
			<< direction_head_vector_filtered[next_ind][ind].x << " "
			<< direction_head_vector_filtered[next_ind][ind].y << " "
			<< v_norm_long_time[next_ind][ind] << " "
			<< v_centroid_long_time[next_ind][ind] << " "
			<< v_norm1[next_ind][ind] << " "
			<< v_norm2[next_ind][ind] << " "
			<< crab_speed1[next_ind][ind] << " "
			<< crab_speed2[next_ind][ind] << " "
			<< angle_upper_lower1[next_ind][ind] << " "
			<< angle_upper_lower2[next_ind][ind] << " "
			<< s1[next_ind][ind] << " "
			<< s2[next_ind][ind] << " "
			<< asymmetry1[next_ind][ind] << " "
			<< asymmetry2[next_ind][ind] << " "
			<< eig_reduced1[next_ind][ind] << " "
			<< eig_reduced2[next_ind][ind] << " "
			<< skeleton_length1[next_ind][ind] << " "
			<< skeleton_length2[next_ind][ind] << " "
			<< perimeter1[next_ind][ind] << " "
			<< perimeter2[next_ind][ind] << " "
			<< damped_distance1[next_ind][ind] << " "
			<< damped_distance2[next_ind][ind] << " "
			<< speed_reduced1[next_ind][ind] << " "
			<< speed_reduced2[next_ind][ind] << " "
			<< parallel_speed1[next_ind][ind] << " "
			<< parallel_speed2[next_ind][ind] << " "
			<< parallel_speed_tail1[next_ind][ind] << " "
			<< parallel_speed_tail2[next_ind][ind] << " "
			<< v_norm_convolved_squared[next_ind][ind] << " "
			<< crab_speed_convolved_squared[next_ind][ind] << " "
			<< angle_upper_lower_convolved_squared[next_ind][ind] << " "
			<< s_convolved_squared[next_ind][ind] << " "
			<< asymmetry_convolved_squared[next_ind][ind] << " "
			<< eig_reduced_convolved_squared[next_ind][ind] << " "
			<< skeleton_length_convolved_squared[next_ind][ind] << " "
			<< perimeter_convolved_squared[next_ind][ind] << " "
			<< damped_distance_convolved_squared[next_ind][ind] << " "
			<< speed_reduced_convolved_squared[next_ind][ind] << " "
			<< parallel_speed_convolved_squared[next_ind][ind] << " "
			<< parallel_speed_tail_convolved_squared[next_ind][ind] << " "
			<< d_roll_smooth[next_ind][ind] << " "
			//<< d_left_smooth[next_ind][ind] << " "
			//<< d_right_smooth[next_ind][ind] << " "
			<< d_bend_smooth[next_ind][ind] << " "
			<< bend[next_ind][ind] << " "
			<< ball[next_ind][ind] << " "
			<< left[next_ind][ind] << " "
			<< right[next_ind][ind] << " "
			<< back[next_ind][ind] << " "
			<< roll[next_ind][ind] << " "
			<< forward[next_ind][ind] << " "
			<< forward_peristaltic[next_ind][ind] << " "
			<< weird[next_ind][ind] << " "
			<< roll_weird[next_ind][ind] << " "
			<< roll_smooth[next_ind][ind] << " "
			//<< left_smooth[next_ind][ind] << " "
			//<< right_smooth[next_ind][ind] << " "
			<< bend_smooth[next_ind][ind] << " "
			<< time_stamp_last_ball[ind] << " "
			<< time_stamp_last_roll[ind] << " "
			<< larvaeValid[ind] << " "
			<< flippedHeadTail[ind] << " "
			<< votes_correct[ind] << " "
			<< votes_flipped[ind] << " "
			<< experimentStart << " "
			<< experimentStop << " "
			<< dt_eff << " "
			<< std::endl;
	}
}


// Write .config file with meta data
void writeConfigFile(std::string dateTime, std::string genotype1, std::string genotype2, std::string stimulationMode, std::string rigName, int lightIntensity, int laserIntensity,
	std::string yokedFileName, std::string stimLabels, double PixelResolution, int ImageHeight, int ImageWidth, int PixelIntensityLow, int PixelIntensityHigh,
	int BoxSideLow, int BoxSideHigh, int BoxWLLow, int BoxWLHigh, int AreaLow, int AreaHigh, int BGSubTH, int minContourPoints, double stim1, double stim2, double stim3, double stim4, double stim5, double stim6, double stim7,
	double stim8, double stim9, double stim10)
{
	// open config file and overwrite existing file if any
	std::string configPath(savingPath);
	configPath = configPath + "\\" + dateTime
		+ "@" + genotype1 + "@" + genotype2
		+ "@" + rigName + "@" + stimulationMode + "@" + std::to_string(lightIntensity) + ".config";
	configFile.open(configPath, std::ios::trunc);
	configFile.close();
	configFile.clear();
	configFile.open(configPath, std::ios::app);

	// write meta data to config file
	configFile
		<< "Date: " << dateTime.substr(0, 8) << "\n"
		<< "Time: " << dateTime.substr(9, 6) << "\n"
		<< "Stimulation Mode: " << stimulationMode << "\n"
		<< "Genotype1: " << genotype1 << "\n"
		<< "Genotype2: " << genotype2 << "\n"
		<< "Light intensity: " << lightIntensity << "\n"
		<< "Laser intensity: " << double(laserIntensity) / 10000 << "\n"
		<< "Yoked Mode: " << yokedMode << "\n"
		<< "Yoked File Name: " << yokedFileName << "\n"
		<< "Stim Labels: " << stimLabels << "\n"
		<< "Gamma Spine: " << gamma_spine << "\n"
		<< "N Spine: " << nSpine << "\n"
		<< "Pixel Resolution: " << PixelResolution << "\n"
		<< "Image Height: " << ImageHeight << "\n"
		<< "Image Width: " << ImageWidth << "\n"
		<< "Pixel Intensity Low: " << PixelIntensityLow << "\n"
		<< "Pixel Intensity High: " << PixelIntensityHigh << "\n"
		<< "Box Side Low: " << BoxSideLow << "\n"
		<< "Box Side High: " << BoxSideHigh << "\n"
		<< "Box Width Plus Length Low: " << BoxWLLow << "\n"
		<< "Box Width Plus Length High: " << BoxWLHigh << "\n"
		<< "Area Low: " << AreaLow << "\n"
		<< "Area High: " << AreaHigh << "\n"
		<< "Background Subtraction Threshold: " << BGSubTH << "\n"
		<< "Min Contour Points: " << minContourPoints << "\n"
		<< stimLabelsVector[0] << ": " << stim1 << "\n"
		<< stimLabelsVector[1] << ": " << stim2 << "\n"
		<< stimLabelsVector[2] << ": " << stim3 << "\n"
		<< stimLabelsVector[3] << ": " << stim4 << "\n"
		<< stimLabelsVector[4] << ": " << stim5 << "\n"
		<< stimLabelsVector[5] << ": " << stim6 << "\n"
		<< stimLabelsVector[6] << ": " << stim7 << "\n"
		<< stimLabelsVector[7] << ": " << stim8 << "\n"
		<< stimLabelsVector[8] << ": " << stim9 << "\n"
		<< stimLabelsVector[9] << ": " << stim10 << "\n"
		<< "#####" << "\n"
		<< "frame" << "\n"
		<< "larvaeIDsPastIndex" << "\n"
		<< "x_head" << "\n"
		<< "y_head" << "\n"
		<< "x_tail" << "\n"
		<< "y_tail" << "\n"
		<< "x_centroid" << "\n"
		<< "y_centroid" << "\n"
		<< "x_neck" << "\n"
		<< "y_neck" << "\n"
		<< "x_neck_top" << "\n"
		<< "y_neck_top" << "\n"
		<< "x_neck_down" << "\n"
		<< "y_neck_down" << "\n"
		<< "larva_arc_ratio" << "\n"
		<< "larva_area_ratio" << "\n"
		<< "skeleton_length" << "\n"
		<< "perimeter" << "\n"
		<< "angle_upper_lower" << "\n"
		<< "x_direction_vector" << "\n"
		<< "y_direction_vector" << "\n"
		<< "x_direction_head_vector" << "\n"
		<< "y_direction_head_vector" << "\n"
		<< "x_direction_tail_vector" << "\n"
		<< "y_direction_tail_vector" << "\n"
		<< "x_head_velocity" << "\n"
		<< "y_head_velocity" << "\n"
		<< "head_speed" << "\n"
		<< "x_tail_velocity" << "\n"
		<< "y_tail_velocity" << "\n"
		<< "tail_speed" << "\n"
		<< "x_neck_velocity" << "\n"
		<< "y_neck_velocity" << "\n"
		<< "neck_speed" << "\n"
		<< "x_neck_top_velocity" << "\n"
		<< "y_neck_top_velocity" << "\n"
		<< "neck_top_speed" << "\n"
		<< "x_neck_down_velocity" << "\n"
		<< "y_neck_down_velocity" << "\n"
		<< "neck_down_speed" << "\n"
		<< "crab_speed" << "\n"
		<< "v_norm" << "\n"
		<< "v_centroid" << "\n"
		<< "speed_reduced" << "\n"
		<< "s" << "\n"
		<< "eig_reduced" << "\n"
		<< "asymmetry" << "\n"
		<< "damped_distance" << "\n"
		<< "parallel_speed" << "\n"
		<< "parallel_speed_tail" << "\n"
		<< "parallel_speed_tail_raw" << "\n"
		<< "v_norm_filtered" << "\n"
		<< "v_centroid_filtered" << "\n"
		<< "crab_speed_filtered" << "\n"
		<< "angle_upper_lower_filtered" << "\n"
		<< "s_filtered" << "\n"
		<< "asymmetry_filtered" << "\n"
		<< "eig_reduced_filtered" << "\n"
		<< "skeleton_length_filtered" << "\n"
		<< "perimeter_filtered" << "\n"
		<< "damped_distance_filtered" << "\n"
		<< "speed_reduced_filtered" << "\n"
		<< "parallel_speed_filtered" << "\n"
		<< "parallel_speed_tail_filtered" << "\n"
		<< "parallel_speed_tail_raw_filtered" << "\n"
		<< "x_neck_velocity_filtered" << "\n"
		<< "y_neck_velocity_filtered" << "\n"
		<< "x_tail_velocity_filtered" << "\n"
		<< "y_tail_velocity_filtered" << "\n"
		<< "x_head_velocity_filtered" << "\n"
		<< "y_head_velocity_filtered" << "\n"
		<< "x_neck_top_velocity_filtered" << "\n"
		<< "y_neck_top_velocity_filtered" << "\n"
		<< "x_neck_down_velocity_filtered" << "\n"
		<< "y_neck_down_velocity_filtered" << "\n"
		<< "x_direction_vector_filtered" << "\n"
		<< "y_direction_vector_filtered" << "\n"
		<< "x_direction_tail_vector_filtered" << "\n"
		<< "y_direction_tail_vector_filtered" << "\n"
		<< "x_direction_head_vector_filtered" << "\n"
		<< "y_direction_head_vector_filtered" << "\n"
		<< "v_norm_long_time" << "\n"
		<< "v_centroid_long_time" << "\n"
		<< "v_norm1" << "\n"
		<< "v_norm2" << "\n"
		<< "crab_speed1" << "\n"
		<< "crab_speed2" << "\n"
		<< "angle_upper_lower1" << "\n"
		<< "angle_upper_lower2" << "\n"
		<< "s1" << "\n"
		<< "s2" << "\n"
		<< "asymmetry1" << "\n"
		<< "asymmetry2" << "\n"
		<< "eig_reduced1" << "\n"
		<< "eig_reduced2" << "\n"
		<< "skeleton_length1" << "\n"
		<< "skeleton_length2" << "\n"
		<< "perimeter1" << "\n"
		<< "perimeter2" << "\n"
		<< "damped_distance1" << "\n"
		<< "damped_distance2" << "\n"
		<< "speed_reduced1" << "\n"
		<< "speed_reduced2" << "\n"
		<< "parallel_speed1" << "\n"
		<< "parallel_speed2" << "\n"
		<< "parallel_speed_tail1" << "\n"
		<< "parallel_speed_tail2" << "\n"
		<< "v_norm_convolved_squared" << "\n"
		<< "crab_speed_convolved_squared" << "\n"
		<< "angle_upper_lower_convolved_squared" << "\n"
		<< "s_convolved_squared" << "\n"
		<< "asymmetry_convolved_squared" << "\n"
		<< "eig_reduced_convolved_squared" << "\n"
		<< "skeleton_length_convolved_squared" << "\n"
		<< "perimeter_convolved_squared" << "\n"
		<< "damped_distance_convolved_squared" << "\n"
		<< "speed_reduced_convolved_squared" << "\n"
		<< "parallel_speed_convolved_squared" << "\n"
		<< "parallel_speed_tail_convolved_squared" << "\n"
		<< "d_roll_smooth" << "\n"
		//<< "d_left_smooth" << "\n"
		//<< "d_right_smooth" << "\n"
		<< "d_bend_smooth" << "\n"
		<< "bend" << "\n"
		<< "ball" << "\n"
		<< "left" << "\n"
		<< "right" << "\n"
		<< "back" << "\n"
		<< "roll" << "\n"
		<< "forward" << "\n"
		<< "forward_peristaltic" << "\n"
		<< "weird" << "\n"
		<< "roll_weird" << "\n"
		<< "roll_smooth" << "\n"
		//<< "left_smooth" << "\n"
		//<< "right_smooth" << "\n"
		<< "bend_smooth" << "\n"
		<< "time_stamp_last_ball" << "\n"
		<< "time_stamp_last_roll" << "\n"
		<< "larvaeValid" << "\n"
		<< "flippedHeadTail" << "\n"
		<< "votes_correct" << "\n"
		<< "votes_flipped" << "\n"
		<< "experimentStart" << "\n"
		<< "experimentStop" << "\n"
		<< "dt" << "\n"
		<< "#####" << "\n"
		<< "nContour" << "\n"
		<< "tictocdiff" << "\n"
		<< "dmd_signal" << "\n"
		<< "ir_signal";

	// close config file
	configFile.close();
	configFile.clear();
}


// Write .stimulation file
void writeStimulationFile(std::string dateTime, std::vector<int> larvaeIDs, std::vector<int> nContour)
{
	// for all larvae
	for (int ind = 0; ind < larvaeIDs.size(); ++ind) {

		stimulationFile << dateTime << " "
			<< larvaeIDs[ind] << " "
			<< time_stamp[next_ind] << " "
			<< nContour[ind] << " "
			<< tictocdiff << " "
			<< dmdIntensities[ind] << " "
			<< irIntensities[ind] << std::endl;
	}
}


// Write .hull file
void writeHullFile(std::string dateTime, std::vector<int> larvaeIDs, std::vector<std::vector<cv::Point2f> > hull)
{
	// for all larvae
	for (int ind = 0; ind < larvaeIDs.size(); ++ind) {

		hullFile << dateTime << " "
			<< larvaeIDs[ind] << " "
			<< time_stamp[next_ind] << " ";
		for (int i = 0; i < hull[ind].size(); ++i) {
			hullFile << hull[ind][i].x << " ";
			hullFile << hull[ind][i].y << " ";
		}
		hullFile << std::endl;
	}
}


// Write .outline file
void writeContourFile(std::string dateTime, std::vector<int> larvaeIDs, std::vector<std::vector<cv::Point2f> > contour)
{
	// for all larvae
	for (int ind = 0; ind < larvaeIDs.size(); ++ind) {

		contourFile << dateTime << " "
			<< larvaeIDs[ind] << " "
			<< time_stamp[next_ind] << " ";
		for (int i = 0; i < contour[ind].size(); ++i) {
			contourFile << contour[ind][i].x << " ";
			contourFile << contour[ind][i].y << " ";
		}
		contourFile << std::endl;
	}
}


// Write .spine file
void writeSpineFile(std::string dateTime, std::vector<int> larvaeIDs, std::vector<std::vector<cv::Point2f> > spine)
{
	// for all larvae
	for (int ind = 0; ind < larvaeIDs.size(); ++ind) {

		spineFile << dateTime << " "
			<< larvaeIDs[ind] << " "
			<< time_stamp[next_ind] << " ";
		for (int i = 0; i < spine_smooth[ind].size(); ++i) {
			spineFile << spine_smooth[ind][i].x << " ";
			spineFile << spine_smooth[ind][i].y << " ";
		}
		spineFile << std::endl;
	}
}


// Resizes standard double feature
void resizeFeature(std::vector<std::vector<double> > & feature) {

	// initialize newFeature
	std::vector<std::vector<double> > newFeature(nBuffers);
	for (int i = 0; i < nBuffers; ++i) {
		newFeature[i].resize(larvaeIDsPastIndex.size());
	}

	// temporarily store data in newFeature
	for (int i = 0; i < nBuffers; ++i) {
		for (int j = 0; j < larvaeIDsPastIndex.size(); ++j) {
			// if this is a new ID
			if (existentInPast[j] == false) {
				newFeature[i][j] = 0;
			}
			// if the ID was detected previously
			else {
				newFeature[i][j] = feature[i][larvaeIDsPastIndex[j]];
			}
		}
	}

	// resize feature
	for (int i = 0; i < nBuffers; ++i) {
		feature[i].resize(larvaeIDsPastIndex.size());
	}

	// load data into feature
	feature = newFeature;
}


// Resizes cv::Point2f feature
void resizeFeaturePoint2f(std::vector<std::vector<cv::Point2f> > & feature) {

	// initialize newFeature
	std::vector<std::vector<cv::Point2f> > newFeature(nBuffers);
	for (int i = 0; i < nBuffers; ++i) {
		newFeature[i].resize(larvaeIDsPastIndex.size());
	}

	// temporarily store data in newFeature
	for (int i = 0; i < nBuffers; ++i) {
		for (int j = 0; j < larvaeIDsPastIndex.size(); ++j) {
			// if this is a new ID
			if (existentInPast[j] == false) {
				newFeature[i][j].x = 0;
				newFeature[i][j].y = 0;
			}
			// if the ID was detected previously
			else {
				newFeature[i][j] = feature[i][larvaeIDsPastIndex[j]];
			}
		}
	}

	// resize feature
	for (int i = 0; i < nBuffers; ++i) {
		feature[i].resize(larvaeIDsPastIndex.size());
	}

	// load data into feature
	feature = newFeature;
}


// Resize spine
void resizeFeaturePoint2fSpine(std::vector<std::vector<cv::Point2f> > & feature) {

	// initialize newFeature
	std::vector<std::vector<cv::Point2f> > newFeature(larvaeIDsPastIndex.size());
	for (int i = 0; i < larvaeIDsPastIndex.size(); ++i) {
		newFeature[i].resize(nSpine);
	}

	// temporarily store data in newFeature
	for (int i = 0; i < larvaeIDsPastIndex.size(); ++i) {
		for (int j = 0; j < nSpine; ++j) {
			// if this is a new ID
			if (existentInPast[i] == false) {
				newFeature[i][j].x = 0;
				newFeature[i][j].y = 0;
			}
			// if the ID was detected previously
			else {
				newFeature[i][j] = feature[larvaeIDsPastIndex[i]][j];
			}
		}
	}

	// resize feature
	feature.resize(larvaeIDsPastIndex.size());
	for (int i = 0; i < larvaeIDsPastIndex.size(); ++i) {
		feature[i].resize(nSpine);
	}

	// load data into feature
	feature = newFeature;
}


// Resizes bool feature
void resizeFeatureBool(std::vector<std::vector<bool> > & feature) {

	// initialize newFeature
	std::vector<std::vector<bool> > newFeature(nBuffers);
	for (int i = 0; i < nBuffers; ++i) {
		newFeature[i].resize(larvaeIDsPastIndex.size());
	}

	// temporarily store data in newFeature
	for (int i = 0; i < nBuffers; ++i) {
		for (int j = 0; j < larvaeIDsPastIndex.size(); ++j) {
			// if this is a new ID
			if (existentInPast[j] == false) {
				newFeature[i][j] = false;
			}
			// if the ID was detected previously
			else {
				newFeature[i][j] = feature[i][larvaeIDsPastIndex[j]];
			}
		}
	}

	// resize feature
	for (int i = 0; i < nBuffers; ++i) {
		feature[i].resize(larvaeIDsPastIndex.size());
	}

	// load data into feature
	feature = newFeature;
}


// Resizes time stamp feature
void resizeTimeStamp(std::vector<double> & time_stamp_loc) {

	// initialize newTimeStamp
	std::vector<double> newTimeStamp(larvaeIDsPastIndex.size());

	// temporarily store data in newTimeStamp
	for (int j = 0; j < larvaeIDsPastIndex.size(); ++j) {
		// if this is a new ID
		if (existentInPast[j] == false) {
			newTimeStamp[j] = -INFINITY;
		}
		// if the ID was detected previously
		else {
			newTimeStamp[j] = time_stamp_loc[larvaeIDsPastIndex[j]];
		}
	}


	// resize time stamp
	time_stamp_loc.resize(larvaeIDsPastIndex.size());

	// load data into time stamp
	time_stamp_loc = newTimeStamp;
}


// Resizes unbuffered bool feature
void resizeUnbufferedBool(std::vector<bool> & feature) {

	// initialize newFeature
	std::vector<bool> newFeature(larvaeIDsPastIndex.size());

	// temporarily store data in newFeature
	for (int j = 0; j < larvaeIDsPastIndex.size(); ++j) {
		// if this is a new ID
		if (existentInPast[j] == false) {
			newFeature[j] = false;
		}
		// if the ID was detected previously
		else {
			newFeature[j] = feature[larvaeIDsPastIndex[j]];
		}
	}

	// resize feature
	feature.resize(larvaeIDsPastIndex.size());

	// load data into feature
	feature = newFeature;
}


// Resizes unbuffered int feature
void resizeUnbufferedInt(std::vector<int> & feature) {

	// initialize newFeature
	std::vector<int> newFeature(larvaeIDsPastIndex.size());

	// temporarily store data in newFeature
	for (int j = 0; j < larvaeIDsPastIndex.size(); ++j) {
		// if this is a new ID
		if (existentInPast[j] == false) {
			newFeature[j] = 0;
		}
		// if the ID was detected previously
		else {
			newFeature[j] = feature[larvaeIDsPastIndex[j]];
		}
	}

	// resize feature
	feature.resize(larvaeIDsPastIndex.size());

	// load data into feature
	feature = newFeature;
}


// Resizes unbuffered cv::Point2f feature
void resizeUnbufferedPoint2f(std::vector<cv::Point2f> & feature) {

	// initialize newFeature
	std::vector<cv::Point2f> newFeature(larvaeIDsPastIndex.size());

	// temporarily store data in newFeature
	for (int j = 0; j < larvaeIDsPastIndex.size(); ++j) {
		// if this is a new ID
		if (existentInPast[j] == false) {
			newFeature[j].x = 0;
			newFeature[j].y = 0;
		}
		// if the ID was detected previously
		else {
			newFeature[j] = feature[larvaeIDsPastIndex[j]];
		}
	}

	// resize feature
	feature.resize(larvaeIDsPastIndex.size());

	// load data into feature
	feature = newFeature;
}


// Resizes all features to account for updated number of detected larvae
void resizeAllFeatures() {

	resizeFeaturePoint2f(head);
	resizeFeaturePoint2f(tail);
	resizeFeaturePoint2f(centroid);
	resizeFeaturePoint2f(neck);
	resizeFeaturePoint2f(neck_top);
	resizeFeaturePoint2f(neck_down);
	resizeFeature(larva_arc_ratio);
	resizeFeature(larva_area_ratio);
	resizeFeature(skeleton_length);
	resizeFeature(perimeter);
	resizeFeature(angle_upper_lower);
	resizeFeaturePoint2f(direction_vector);
	resizeFeaturePoint2f(direction_head_vector);
	resizeFeaturePoint2f(direction_tail_vector);
	resizeFeaturePoint2f(head_velocity);
	resizeFeature(head_speed);
	resizeFeaturePoint2f(tail_velocity);
	resizeFeature(tail_speed);
	resizeFeaturePoint2f(neck_velocity);
	resizeFeature(neck_speed);
	resizeFeaturePoint2f(neck_top_velocity);
	resizeFeature(neck_top_speed);
	resizeFeaturePoint2f(neck_down_velocity);
	resizeFeature(neck_down_speed);
	resizeFeature(crab_speed);
	resizeFeature(v_norm);
	resizeFeature(v_centroid);
	resizeFeature(speed_reduced);
	resizeFeature(s);
	resizeFeature(eig_reduced);
	resizeFeature(asymmetry);
	resizeFeature(damped_distance);
	resizeFeature(parallel_speed);
	resizeFeature(parallel_speed_tail);
	resizeFeature(parallel_speed_tail_raw);
	resizeFeature(v_norm_filtered);
	resizeFeature(v_centroid_filtered);
	resizeFeature(crab_speed_filtered);
	resizeFeature(angle_upper_lower_filtered);
	resizeFeature(s_filtered);
	resizeFeature(asymmetry_filtered);
	resizeFeature(eig_reduced_filtered);
	resizeFeature(skeleton_length_filtered);
	resizeFeature(perimeter_filtered);
	resizeFeature(damped_distance_filtered);
	resizeFeature(speed_reduced_filtered);
	resizeFeature(parallel_speed_filtered);
	resizeFeature(parallel_speed_tail_filtered);
	resizeFeature(parallel_speed_tail_raw_filtered);
	resizeFeaturePoint2f(neck_velocity_filtered);
	resizeFeaturePoint2f(tail_velocity_filtered);
	resizeFeaturePoint2f(head_velocity_filtered);
	resizeFeaturePoint2f(neck_top_velocity_filtered);
	resizeFeaturePoint2f(neck_down_velocity_filtered);
	resizeFeaturePoint2f(direction_vector_filtered);
	resizeFeaturePoint2f(direction_tail_vector_filtered);
	resizeFeaturePoint2f(direction_head_vector_filtered);
	resizeFeature(v_norm_long_time);
	resizeFeature(v_centroid_long_time);
	resizeFeature(v_norm1);
	resizeFeature(v_norm2);
	resizeFeature(crab_speed1);
	resizeFeature(crab_speed2);
	resizeFeature(angle_upper_lower1);
	resizeFeature(angle_upper_lower2);
	resizeFeature(s1);
	resizeFeature(s2);
	resizeFeature(asymmetry1);
	resizeFeature(asymmetry2);
	resizeFeature(eig_reduced1);
	resizeFeature(eig_reduced2);
	resizeFeature(skeleton_length1);
	resizeFeature(skeleton_length2);
	resizeFeature(perimeter1);
	resizeFeature(perimeter2);
	resizeFeature(damped_distance1);
	resizeFeature(damped_distance2);
	resizeFeature(speed_reduced1);
	resizeFeature(speed_reduced2);
	resizeFeature(parallel_speed1);
	resizeFeature(parallel_speed2);
	resizeFeature(parallel_speed_tail1);
	resizeFeature(parallel_speed_tail2);
	resizeFeature(v_norm_convolved_squared);
	resizeFeature(crab_speed_convolved_squared);
	resizeFeature(angle_upper_lower_convolved_squared);
	resizeFeature(s_convolved_squared);
	resizeFeature(asymmetry_convolved_squared);
	resizeFeature(eig_reduced_convolved_squared);
	resizeFeature(skeleton_length_convolved_squared);
	resizeFeature(perimeter_convolved_squared);
	resizeFeature(damped_distance_convolved_squared);
	resizeFeature(speed_reduced_convolved_squared);
	resizeFeature(parallel_speed_convolved_squared);
	resizeFeature(parallel_speed_tail_convolved_squared);
	resizeFeature(d_roll);
	//resizeFeature(d_left);
	//resizeFeature(d_right);
	resizeFeature(d_bend);
	resizeFeature(d_roll_smooth);
	//resizeFeature(d_left_smooth);
	//resizeFeature(d_right_smooth);
	resizeFeature(d_bend_smooth);
	resizeFeatureBool(bend);
	resizeFeatureBool(ball);
	resizeFeatureBool(left);
	resizeFeatureBool(right);
	resizeFeatureBool(back);
	resizeFeatureBool(roll);
	resizeFeatureBool(forward);
	resizeFeatureBool(forward_peristaltic);
	resizeFeatureBool(weird);
	resizeFeatureBool(roll_weird);
	resizeFeatureBool(roll_smooth);
	//resizeFeatureBool(left_smooth);
	//resizeFeatureBool(right_smooth);
	resizeFeatureBool(bend_smooth);
	resizeTimeStamp(time_stamp_last_ball);
	resizeTimeStamp(time_stamp_last_roll);
	resizeUnbufferedBool(last_bend_direction);
	resizeUnbufferedBool(existentAndValidInPast);
	resizeUnbufferedBool(validInPresentOrPast);
	resizeUnbufferedBool(flippedHeadTail);
	resizeUnbufferedBool(stimulating);
	resizeUnbufferedInt(votes_correct);
	resizeUnbufferedInt(votes_flipped);
	resizeUnbufferedPoint2f(master_head);
	resizeUnbufferedPoint2f(master_tail);
}


// Called by splitStringByDelimiter
template<typename Out>
void split(const std::string &s, char delim, Out result) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		*(result++) = item;
	}
}


// Split string by delimiter
std::vector<std::string> splitStringByDelimiter(const std::string &s, char delim) {
	// Adapted from https://stackoverflow.com/questions/236129/the-most-elegant-way-to-iterate-the-words-of-a-string
	std::vector<std::string> elems;
	split(s, delim, std::back_inserter(elems));
	return elems;
}


// Reads meta data
void readMetaData(char *dateTimePtr, int dateTimeLength, char *genotype1Ptr, int genotype1Length, char *genotype2Ptr, int genotype2Length,
	char *stimulationModePtr, int stimulationModeLength, char *rigNamePtr, int rigNameLength, char* savingPathPtr, int savingPathLength, char* yokedFileNamePtr, int yokedFileNameLength,
	char* stimLabelsPtr, int stimLabelsLength, std::string & dateTime, std::string & genotype1, std::string & genotype2, std::string & stimulationMode, std::string & rigName, std::string & savingPathLoc, std::string & yokedFileName, std::string & stimLabels,
	int yokedFileEnable, double gamma_loc, int nSpineLoc)
{
	for (int i = 0; i<dateTimeLength; ++i) {
		dateTime.push_back(*(dateTimePtr + i));
	}

	for (int i = 0; i<genotype1Length; ++i) {
		genotype1.push_back(*(genotype1Ptr + i));
	}

	for (int i = 0; i<genotype2Length; ++i) {
		genotype2.push_back(*(genotype2Ptr + i));
	}

	for (int i = 0; i<stimulationModeLength; ++i) {
		stimulationMode.push_back(*(stimulationModePtr + i));
	}

	for (int i = 0; i<rigNameLength; ++i) {
		rigName.push_back(*(rigNamePtr + i));
	}

	for (int i = 0; i<savingPathLength; ++i) {
		savingPathLoc.push_back(*(savingPathPtr + i));
	}

	for (int i = 0; i<yokedFileNameLength; ++i) {
		yokedFileName.push_back(*(yokedFileNamePtr + i));
	}

	for (int i = 0; i<stimLabelsLength; ++i) {
		stimLabels.push_back(*(stimLabelsPtr + i));
	}
	stimLabelsVector = splitStringByDelimiter(stimLabels, ',');

	gamma_spine = gamma_loc;
	nSpine = nSpineLoc;
}


// Reads data
void readData(int numLarvae, int *larvaeIDsPtr, int *nContourPtr, int *xContourPtr, int *yContourPtr,
	int *xSpinePtr, int *ySpinePtr, int *larvaeValidPtr, std::vector<int> & larvaeIDs, std::vector<int> & nContour,
	std::vector<std::vector<cv::Point2f> > & contour, std::vector<std::vector<cv::Point2f> > & spine, std::vector<std::vector<cv::Point2f> > & hull,
	std::vector<bool> & larvaeValid, int ImageHeight, double PixelResolution, int *flipLarvaeIDPtr, int flipLarvaeIDLength)
{
	// by default, 16 objects are sent to this code, but some of them are not actual larvae but instead placeholders (larvaeID is 0) which need to be removed (i.e. "corrected")

	// dereference pointers
	std::vector<int> larvaeIDsUncorrected;
	std::vector<int> nContourUncorrected;
	std::vector<bool> larvaeValidUncorrected;
	larvaeIDsUncorrected.resize(numLarvae);
	nContourUncorrected.resize(numLarvae);
	larvaeValidUncorrected.resize(numLarvae);
	int nContourSum = 0;
	for (int i = 0; i < numLarvae; ++i) {
		larvaeIDsUncorrected[i] = *(larvaeIDsPtr + i);
		nContourUncorrected[i] = *(nContourPtr + i);
		nContourSum += nContourUncorrected[i];
		if (*(larvaeValidPtr + i) == 0) {
			larvaeValidUncorrected[i] = false;
		}
		else {
			larvaeValidUncorrected[i] = true;
		}
	}

	std::vector<double> xContour;
	std::vector<double> yContour;
	xContour.resize(nContourSum);
	yContour.resize(nContourSum);
	for (int i = 0; i<nContourSum; ++i) {
		// convert from camera image pixels to mm
		xContour[i] = *(xContourPtr + i) / 1000.0;
		yContour[i] = (ImageHeight*PixelResolution - *(yContourPtr + i)) / 1000.0;
	}

	std::vector<double> xSpine;
	std::vector<double> ySpine;
	xSpine.resize(nSpine*numLarvae);
	ySpine.resize(nSpine*numLarvae);
	for (int i = 0; i<numLarvae*nSpine; ++i) {
		// convert from camera image pixels to mm
		xSpine[i] = *(xSpinePtr + i) / 1000.0;
		ySpine[i] = (ImageHeight*PixelResolution - *(ySpinePtr + i)) / 1000.0;
	}

	std::vector<double> xContourTemp;
	std::vector<double> yContourTemp;
	std::vector<double> xSpineTemp;
	std::vector<double> ySpineTemp;
	xSpineTemp.resize(nSpine);
	ySpineTemp.resize(nSpine);

	int numLarvaeCorrected = 0;
	for (int i = 0; i < numLarvae; ++i) {
		if (larvaeIDsUncorrected[i] > 0) {
			++numLarvaeCorrected;
		}
	}
	numLarvae = numLarvaeCorrected;
	spine.resize(numLarvae);
	contour.resize(numLarvae);
	hull.resize(numLarvae);
	larvaeIDs.resize(numLarvae);
	nContour.resize(numLarvae);
	larvaeValid.resize(numLarvae);

	// read flipped IDs
	flipLarvaeID.resize(numLarvae);
	std::vector<int> flipTemp(flipLarvaeIDLength);
	for (int i = 0; i < flipLarvaeIDLength; ++i) {
		flipTemp[i] = *(flipLarvaeIDPtr + i);
	}

	// initialize Fourier coefficients
	int contourFirstIndex = 0;
	std::vector<std::vector<std::vector<double> > > fourierCoeffs(numLarvae);

	// iterate through all objects (including placeholders) to extract data for actual objects
	int k = 0; // index of actual objects
	for (int i = 0; i<numLarvae; ++i) {

		int nContourTemp = nContourUncorrected[i];

		if (larvaeIDsUncorrected[i] > 0) {

			larvaeIDs[k] = larvaeIDsUncorrected[i];
			nContour[k] = nContourUncorrected[i];
			larvaeValid[k] = larvaeValidUncorrected[i];
			spine[k].resize(nSpine);
			contour[k].resize(nContourReconstr);

			// extract contour for this larva
			xContourTemp.resize(nContour[k]);
			yContourTemp.resize(nContour[k]);
			for (int j = 0; j<nContour[k]; ++j) {
				xContourTemp[j] = xContour[contourFirstIndex + j];
				yContourTemp[j] = yContour[contourFirstIndex + j];
			}

			// Fourier decomposition of contour
			fourierCoeffs[k] = fourierDecompose(xContourTemp, yContourTemp, nContourTemp);

			// extract spine for this larva
			for (int j = 0; j<nSpine; ++j) {
				xSpineTemp[j] = xSpine[i*nSpine + j];
				ySpineTemp[j] = ySpine[i*nSpine + j];

				spine[k][j].x = xSpineTemp[j];
				spine[k][j].y = ySpineTemp[j];
			}

			// flag this larva if head tail votes were flipped
			flipLarvaeID[k] = (std::find(flipTemp.begin(), flipTemp.end(), larvaeIDs[k]) != flipTemp.end());

			++k;
		}

		contourFirstIndex += nContourTemp;
	}

	// Fourier reconstruction of contours
	fourierReconstruct(fourierCoeffs, contour);
}


// Locks standard double feature
void lockFeature(std::vector<std::vector<double> > & feature, int ind)
{
	int next_ind_loc;
	if (next_ind == 0)
	{
		next_ind_loc = nBuffers;
	}
	else
	{
		next_ind_loc = next_ind;
	}

	feature[next_ind][ind] = feature[next_ind_loc - 1][ind];
}


// Locks cv::Point2f feature
void lockFeaturePoint2f(std::vector<std::vector<cv::Point2f> > & feature, int ind)
{
	int next_ind_loc;
	if (next_ind == 0)
	{
		next_ind_loc = nBuffers;
	}
	else
	{
		next_ind_loc = next_ind;
	}

	feature[next_ind][ind] = feature[next_ind_loc - 1][ind];
}

// Locks Boolean feature
void lockFeatureBool(std::vector<std::vector<bool> > & feature, int ind)
{
	int next_ind_loc;
	if (next_ind == 0)
	{
		next_ind_loc = nBuffers;
	}
	else
	{
		next_ind_loc = next_ind;
	}

	feature[next_ind][ind] = feature[next_ind_loc - 1][ind];
}


// Assign all features the value of the previous frame
void lockAllFeatures(int ind)
{
	lockFeaturePoint2f(head, ind);
	lockFeaturePoint2f(tail, ind);
	lockFeaturePoint2f(centroid, ind);
	lockFeaturePoint2f(neck, ind);
	lockFeaturePoint2f(neck_top, ind);
	lockFeaturePoint2f(neck_down, ind);
	lockFeature(larva_arc_ratio, ind);
	lockFeature(larva_area_ratio, ind);
	lockFeature(skeleton_length, ind);
	lockFeature(perimeter, ind);
	lockFeature(angle_upper_lower, ind);
	lockFeaturePoint2f(direction_vector, ind);
	lockFeaturePoint2f(direction_head_vector, ind);
	lockFeaturePoint2f(direction_tail_vector, ind);
	lockFeaturePoint2f(head_velocity, ind);
	lockFeature(head_speed, ind);
	lockFeaturePoint2f(tail_velocity, ind);
	lockFeature(tail_speed, ind);
	lockFeaturePoint2f(neck_velocity, ind);
	lockFeature(neck_speed, ind);
	lockFeaturePoint2f(neck_top_velocity, ind);
	lockFeature(neck_top_speed, ind);
	lockFeaturePoint2f(neck_down_velocity, ind);
	lockFeature(neck_down_speed, ind);
	lockFeature(crab_speed, ind);
	lockFeature(v_norm, ind);
	lockFeature(v_centroid, ind);
	lockFeature(speed_reduced, ind);
	lockFeature(s, ind);
	lockFeature(eig_reduced, ind);
	lockFeature(asymmetry, ind);
	lockFeature(damped_distance, ind);
	lockFeature(parallel_speed, ind);
	lockFeature(parallel_speed_tail, ind);
	lockFeature(parallel_speed_tail_raw, ind);
	lockFeature(v_norm_filtered, ind);
	lockFeature(v_centroid_filtered, ind);
	lockFeature(crab_speed_filtered, ind);
	lockFeature(angle_upper_lower_filtered, ind);
	lockFeature(s_filtered, ind);
	lockFeature(asymmetry_filtered, ind);
	lockFeature(eig_reduced_filtered, ind);
	lockFeature(skeleton_length_filtered, ind);
	lockFeature(perimeter_filtered, ind);
	lockFeature(damped_distance_filtered, ind);
	lockFeature(speed_reduced_filtered, ind);
	lockFeature(parallel_speed_filtered, ind);
	lockFeature(parallel_speed_tail_filtered, ind);
	lockFeature(parallel_speed_tail_raw_filtered, ind);
	lockFeaturePoint2f(neck_velocity_filtered, ind);
	lockFeaturePoint2f(tail_velocity_filtered, ind);
	lockFeaturePoint2f(head_velocity_filtered, ind);
	lockFeaturePoint2f(neck_top_velocity_filtered, ind);
	lockFeaturePoint2f(neck_down_velocity_filtered, ind);
	lockFeaturePoint2f(direction_vector_filtered, ind);
	lockFeaturePoint2f(direction_tail_vector_filtered, ind);
	lockFeaturePoint2f(direction_head_vector_filtered, ind);
	lockFeature(v_norm_long_time, ind);
	lockFeature(v_centroid_long_time, ind);
	lockFeature(v_norm1, ind);
	lockFeature(v_norm2, ind);
	lockFeature(crab_speed1, ind);
	lockFeature(crab_speed2, ind);
	lockFeature(angle_upper_lower1, ind);
	lockFeature(angle_upper_lower2, ind);
	lockFeature(s1, ind);
	lockFeature(s2, ind);
	lockFeature(asymmetry1, ind);
	lockFeature(asymmetry2, ind);
	lockFeature(eig_reduced1, ind);
	lockFeature(eig_reduced2, ind);
	lockFeature(skeleton_length1, ind);
	lockFeature(skeleton_length2, ind);
	lockFeature(perimeter1, ind);
	lockFeature(perimeter2, ind);
	lockFeature(damped_distance1, ind);
	lockFeature(damped_distance2, ind);
	lockFeature(speed_reduced1, ind);
	lockFeature(speed_reduced2, ind);
	lockFeature(parallel_speed1, ind);
	lockFeature(parallel_speed2, ind);
	lockFeature(parallel_speed_tail1, ind);
	lockFeature(parallel_speed_tail2, ind);
	lockFeature(v_norm_convolved_squared, ind);
	lockFeature(crab_speed_convolved_squared, ind);
	lockFeature(angle_upper_lower_convolved_squared, ind);
	lockFeature(s_convolved_squared, ind);
	lockFeature(asymmetry_convolved_squared, ind);
	lockFeature(eig_reduced_convolved_squared, ind);
	lockFeature(skeleton_length_convolved_squared, ind);
	lockFeature(perimeter_convolved_squared, ind);
	lockFeature(damped_distance_convolved_squared, ind);
	lockFeature(speed_reduced_convolved_squared, ind);
	lockFeature(parallel_speed_convolved_squared, ind);
	lockFeature(parallel_speed_tail_convolved_squared, ind);
	lockFeature(d_roll, ind);
	//lockFeature(d_left, ind);
	//lockFeature(d_right, ind);
	lockFeature(d_bend, ind);
	lockFeature(d_roll_smooth, ind);
	//lockFeature(d_left_smooth, ind);
	//lockFeature(d_right_smooth, ind);
	lockFeature(d_bend_smooth, ind);
	lockFeatureBool(bend, ind);
	lockFeatureBool(ball, ind);
	lockFeatureBool(left, ind);
	lockFeatureBool(right, ind);
	lockFeatureBool(back, ind);
	lockFeatureBool(roll, ind);
	lockFeatureBool(forward, ind);
	lockFeatureBool(forward_peristaltic, ind);
	lockFeatureBool(weird, ind);
	lockFeatureBool(roll_weird, ind);
	lockFeatureBool(roll_smooth, ind);
	//lockFeatureBool(left_smooth, ind);
	//lockFeatureBool(right_smooth, ind);
	lockFeatureBool(bend_smooth, ind);
}


// Get area of the contour using shoelace formula
double getArea(std::vector<cv::Point2f> contour_loc)
{
	double area = (contour_loc[0].x - contour_loc[contour_loc.size() - 1].x) * (contour_loc[0].y + contour_loc[contour_loc.size() - 1].y);

	for (int i = 1; i < contour_loc.size(); ++i) {
		area += (contour_loc[i].x - contour_loc[i - 1].x) * (contour_loc[i].y + contour_loc[i - 1].y);
	}

	if (area < 0) {
		area = -area;
	}
	return area / 2;
}


// Get arc length (perimeter) of the contour
double getArcLength(std::vector<cv::Point2f> contour_loc)
{
	double dx, dy;
	dx = contour_loc[0].x - contour_loc[contour_loc.size() - 1].x;
	dy = contour_loc[0].y - contour_loc[contour_loc.size() - 1].y;
	double arc_length = sqrt(dx * dx + dy * dy);

	for (int i = 1; i < contour_loc.size(); ++i) {
		dx = contour_loc[i].x - contour_loc[i - 1].x;
		dy = contour_loc[i].y - contour_loc[i - 1].y;
		arc_length += sqrt(dx * dx + dy * dy);
	}

	return arc_length;
}


// Define Point struct used to obtain the convex hull of a polygon
struct Point final {

public: double x;
public: double y;


public: bool operator==(const Point &other) const;
public: bool operator!=(const Point &other) const;
public: bool operator< (const Point &other) const;
public: bool operator<=(const Point &other) const;
public: bool operator> (const Point &other) const;
public: bool operator>=(const Point &other) const;

};


// Returns a new list of points representing the convex hull of
// the given set of points. The convex hull excludes collinear points.
// This algorithm runs in O(n log n) time.
std::vector<Point> makeConvexHull(const std::vector<Point> &points);


// Returns the convex hull, assuming that each points[i] <= points[i + 1]. Runs in O(n) time.
std::vector<Point> makeConvexHullPresorted(const std::vector<Point> &points);


// Operator used to obtain the convex hull of a polygon
bool Point::operator==(const Point &other) const {
	return x == other.x && y == other.y;
}


// Operator used to obtain the convex hull of a polygon
bool Point::operator!=(const Point &other) const {
	return x != other.x || y != other.y;
}


// Operator used to obtain the convex hull of a polygon
bool Point::operator<(const Point &other) const {
	if (x != other.x) return x < other.x;
	else return y < other.y;
}


// Operator used to obtain the convex hull of a polygon
bool Point::operator>(const Point &other) const {
	if (x != other.x) return x > other.x;
	else return y > other.y;
}


// Operator used to obtain the convex hull of a polygon
bool Point::operator<=(const Point &other) const {
	if (x != other.x) return x < other.x;
	else return y <= other.y;
}


// Operator used to obtain the convex hull of a polygon
bool Point::operator>=(const Point &other) const {
	if (x != other.x) return x > other.x;
	else return y >= other.y;
}


// Called by getConvexHull
std::vector<Point> makeConvexHull(const std::vector<Point> &points) {
	std::vector<Point> newPoints = points;
	std::sort(newPoints.begin(), newPoints.end());
	return makeConvexHullPresorted(newPoints);
}


// Called by makeConvexHull
std::vector<Point> makeConvexHullPresorted(const std::vector<Point> &points) {
	if (points.size() <= 1)
		return std::vector<Point>(points);

	// Andrew's monotone chain algorithm. Positive y coordinates correspond to "up"
	// as per the mathematical convention, instead of "down" as per the computer
	// graphics convention. This doesn't affect the correctness of the result.

	std::vector<Point> upperHull;
	for (const Point &p : points) {
		while (upperHull.size() >= 2) {
			const Point &q = *(upperHull.cend() - 1);  // Same as .back()
			const Point &r = *(upperHull.cend() - 2);
			if ((q.x - r.x) * (p.y - r.y) >= (q.y - r.y) * (p.x - r.x))
				upperHull.pop_back();
			else
				break;
		}
		upperHull.push_back(p);
	}
	upperHull.pop_back();

	std::vector<Point> lowerHull;
	for (std::vector<Point>::const_reverse_iterator it = points.crbegin(); it != points.crend(); ++it) {
		const Point &p = *it;
		while (lowerHull.size() >= 2) {
			const Point &q = *(lowerHull.cend() - 1);  // Same as .back()
			const Point &r = *(lowerHull.cend() - 2);
			if ((q.x - r.x) * (p.y - r.y) >= (q.y - r.y) * (p.x - r.x))
				lowerHull.pop_back();
			else
				break;
		}
		lowerHull.push_back(p);
	}
	lowerHull.pop_back();

	if (!(upperHull.size() == 1 && upperHull == lowerHull))
		upperHull.insert(upperHull.end(), lowerHull.cbegin(), lowerHull.cend());
	return upperHull;
}


// Get convex hull of a polygon
std::vector<cv::Point2f> getConvexHull(std::vector<cv::Point2f> contour_loc)
{
	/*
	* Convex hull algorithm - Library (C++)
	*
	* Copyright (c) 2017 Project Nayuki
	* https://www.nayuki.io/page/convex-hull-algorithm
	*
	* This program is free software: you can redistribute it and/or modify
	* it under the terms of the GNU Lesser General Public License as published by
	* the Free Software Foundation, either version 3 of the License, or
	* (at your option) any later version.
	*
	* This program is distributed in the hope that it will be useful,
	* but WITHOUT ANY WARRANTY; without even the implied warranty of
	* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	* GNU Lesser General Public License for more details.
	*
	* You should have received a copy of the GNU Lesser General Public License
	* along with this program (see COPYING.txt and COPYING.LESSER.txt).
	* If not, see <http://www.gnu.org/licenses/>.
	*/
	// modified by Kristina Klein to fit std::vector<cv::Point2f>

	std::vector<Point> contour_points(contour_loc.size());
	for (int i = 0; i < contour_loc.size(); ++i) {
		contour_points[i].x = contour_loc[i].x;
		contour_points[i].y = contour_loc[i].y;
	}

	std::vector<Point> hull_points = makeConvexHull(contour_points);
	std::vector<cv::Point2f> hull_loc(hull_points.size());
	for (int i = 0; i < hull_points.size(); ++i) {
		hull_loc[i].x = hull_points[i].x;
		hull_loc[i].y = hull_points[i].y;
	}
	return hull_loc;
}


// Calculate larva_arc_ratio and larva_area_ratio
void getContourHullRatios(int ind, std::vector<cv::Point2f> contour_loc, std::vector<cv::Point2f> & hull_loc)
{
	double larva_arc_length;
	double larva_contour_area;
	double larva_hull_arc_length;
	double larva_hull_contour_area;

	// get perimeter and area of larval contour
	perimeter[next_ind][ind] = getArcLength(contour_loc);
	larva_contour_area = getArea(contour_loc);

	// get convex hull of larval contour
	hull_loc = getConvexHull(contour_loc);

	// get perimeter and area of convex hull
	larva_hull_arc_length = getArcLength(hull_loc);
	larva_hull_contour_area = getArea(hull_loc);

	// calculate ratios
	larva_arc_ratio[next_ind][ind] = perimeter[next_ind][ind] / larva_hull_arc_length;
	larva_area_ratio[next_ind][ind] = larva_contour_area / larva_hull_contour_area;
}


// get landmark points on spine and the centroid
void getLandmarkPoints(int ind, std::vector<cv::Point2f> spine_loc, std::vector<cv::Point2f> contour_loc)
{
	// algorithm works for n_spine >= 5
	double run_len;
	double neck_percentage = 0.5; // defines location of neck on spine
	double half_neck_percentage = 0.5*neck_percentage;

	// assign head and tail
	head[next_ind][ind] = spine_loc[0];
	tail[next_ind][ind] = spine_loc[nSpine - 1];

	// calculate skeleton length
	double skel_len = 0;
	for (int i = 1; i<spine_loc.size(); ++i)
	{
		skel_len += sqrt(pow(spine_loc[i].x - spine_loc[i - 1].x, 2) + pow(spine_loc[i].y - spine_loc[i - 1].y, 2));
	}
	skeleton_length[next_ind][ind] = skel_len;

	// find centroid
	double x_centroid = 0;
	double y_centroid = 0;
	for (int i = 0; i < contour_loc.size(); ++i)
	{
		x_centroid += contour_loc[i].x;
		y_centroid += contour_loc[i].y;
	}
	centroid[next_ind][ind].x = x_centroid / contour_loc.size();
	centroid[next_ind][ind].y = y_centroid / contour_loc.size();

	// find neck
	run_len = 0;
	int neck_ind;
	bool found_neck = false;
	for (int i = 1; i<spine_loc.size() - 2; ++i)
	{
		run_len += sqrt(pow(spine_loc[i].x - spine_loc[i - 1].x, 2) + pow(spine_loc[i].y - spine_loc[i - 1].y, 2));
		if (run_len >(neck_percentage * skel_len) && i > 1) // ensures that neck_top and neck_down can be distinct from neck, head and tail
		{
			neck_ind = i;
			neck[next_ind][ind] = spine_loc[neck_ind];
			found_neck = true;
			break;
		}
	}
	if (!found_neck) {
		neck_ind = int(spine_loc.size() / 2);
		neck[next_ind][ind] = spine_loc[neck_ind]; // set neck to middle spine point if no other candidate was found
	}

	// find neck_top
	run_len = 0;
	int neck_top_ind;
	bool found_neck_top = false;
	for (int i = 1; i<spine_loc.size(); ++i)
	{
		run_len += sqrt(pow(spine_loc[i].x - spine_loc[i - 1].x, 2) + pow(spine_loc[i].y - spine_loc[i - 1].y, 2));
		if (run_len >(half_neck_percentage * skel_len) && i < neck_ind)
		{
			neck_top_ind = i;
			neck_top[next_ind][ind] = spine_loc[neck_top_ind];
			found_neck_top = true;
			break;
		}
	}
	if (!found_neck_top) {
		neck_top_ind = int(neck_ind / 2);
		neck_top[next_ind][ind] = spine_loc[neck_top_ind];
	}

	// find neck_down
	run_len = 0;
	int neck_down_ind;
	bool found_neck_down = false;
	for (int i = 1; i<spine_loc.size() - 1; ++i)
	{
		run_len += sqrt(pow(spine_loc[i].x - spine_loc[i - 1].x, 2) + pow(spine_loc[i].y - spine_loc[i - 1].y, 2));
		if (run_len >((1.0 - half_neck_percentage) * skel_len) && i > neck_ind)
		{
			neck_down_ind = i;
			neck_down[next_ind][ind] = spine_loc[neck_down_ind];
			found_neck_down = true;
			break;
		}
	}
	if (!found_neck_down) {
		neck_down_ind = int((neck_ind + spine_loc.size() - 1) / 2);
		neck_down[next_ind][ind] = spine_loc[neck_down_ind];
	}
}


// Calculate direction vectors and angles between them
void giveDirectionVectorsAndAngles(int ind)
{
	double norm;

	// get direction_vector
	direction_vector[next_ind][ind] = neck[next_ind][ind] - neck_down[next_ind][ind];
	norm = sqrt(direction_vector[next_ind][ind].x*direction_vector[next_ind][ind].x + direction_vector[next_ind][ind].y*direction_vector[next_ind][ind].y);
	direction_vector[next_ind][ind].x = direction_vector[next_ind][ind].x / norm;
	direction_vector[next_ind][ind].y = direction_vector[next_ind][ind].y / norm;

	// get direction_head_vector
	direction_head_vector[next_ind][ind] = head[next_ind][ind] - neck_top[next_ind][ind];
	norm = sqrt(direction_head_vector[next_ind][ind].x*direction_head_vector[next_ind][ind].x + direction_head_vector[next_ind][ind].y*direction_head_vector[next_ind][ind].y);
	direction_head_vector[next_ind][ind].x = direction_head_vector[next_ind][ind].x / norm;
	direction_head_vector[next_ind][ind].y = direction_head_vector[next_ind][ind].y / norm;

	// get direction_tail_vector
	direction_tail_vector[next_ind][ind] = neck_down[next_ind][ind] - tail[next_ind][ind];
	norm = sqrt(direction_tail_vector[next_ind][ind].x*direction_tail_vector[next_ind][ind].x + direction_tail_vector[next_ind][ind].y*direction_tail_vector[next_ind][ind].y);
	direction_tail_vector[next_ind][ind].x = direction_tail_vector[next_ind][ind].x / norm;
	direction_tail_vector[next_ind][ind].y = direction_tail_vector[next_ind][ind].y / norm;

	// get angles
	angle_upper_lower[next_ind][ind] = acos(direction_vector[next_ind][ind].x * direction_head_vector[next_ind][ind].x + direction_vector[next_ind][ind].y * direction_head_vector[next_ind][ind].y);
	asymmetry[next_ind][ind] = direction_vector[next_ind][ind].x*direction_head_vector[next_ind][ind].y - direction_vector[next_ind][ind].y*direction_head_vector[next_ind][ind].x;
}


// Calculate velocity features
void calcVelocities(int ind)
{
	// get index in buffer
	int next_ind_loc;
	if (velocity_step > next_ind)
	{
		next_ind_loc = next_ind + nBuffers;
	}
	else
	{
		next_ind_loc = next_ind;
	}

	// get time interval
	double dt = time_stamp[next_ind] - time_stamp[next_ind_loc - velocity_step]; //time interval into history in seconds

																				 // head velocity [mm/s]																			 // head velocity [mm/s]
	head_velocity[next_ind][ind].x = (head[next_ind][ind].x - head[next_ind_loc - velocity_step][ind].x) / dt;
	head_velocity[next_ind][ind].y = (head[next_ind][ind].y - head[next_ind_loc - velocity_step][ind].y) / dt;
	head_speed[next_ind][ind] = sqrt(pow(head_velocity[next_ind][ind].x, 2) + pow(head_velocity[next_ind][ind].y, 2));

	// tail velocity [mm/s]
	tail_velocity[next_ind][ind].x = (tail[next_ind][ind].x - tail[next_ind_loc - velocity_step][ind].x) / dt;
	tail_velocity[next_ind][ind].y = (tail[next_ind][ind].y - tail[next_ind_loc - velocity_step][ind].y) / dt;
	tail_speed[next_ind][ind] = sqrt(pow(tail_velocity[next_ind][ind].x, 2) + pow(tail_velocity[next_ind][ind].y, 2));

	// neck velocity [mm/s]
	neck_velocity[next_ind][ind].x = (neck[next_ind][ind].x - neck[next_ind_loc - velocity_step][ind].x) / dt;
	neck_velocity[next_ind][ind].y = (neck[next_ind][ind].y - neck[next_ind_loc - velocity_step][ind].y) / dt;
	neck_speed[next_ind][ind] = sqrt(pow(neck_velocity[next_ind][ind].x, 2) + pow(neck_velocity[next_ind][ind].y, 2));

	// neck top velocity [mm/s]
	neck_top_velocity[next_ind][ind].x = (neck_top[next_ind][ind].x - neck_top[next_ind_loc - velocity_step][ind].x) / dt;
	neck_top_velocity[next_ind][ind].y = (neck_top[next_ind][ind].y - neck_top[next_ind_loc - velocity_step][ind].y) / dt;
	neck_top_speed[next_ind][ind] = sqrt(pow(neck_top_velocity[next_ind][ind].x, 2) + pow(neck_top_velocity[next_ind][ind].y, 2));

	// neck down velocity [mm/s]
	neck_down_velocity[next_ind][ind].x = (neck_down[next_ind][ind].x - neck_down[next_ind_loc - velocity_step][ind].x) / dt;
	neck_down_velocity[next_ind][ind].y = (neck_down[next_ind][ind].y - neck_down[next_ind_loc - velocity_step][ind].y) / dt;
	neck_down_speed[next_ind][ind] = sqrt(pow(neck_down_velocity[next_ind][ind].x, 2) + pow(neck_down_velocity[next_ind][ind].y, 2));

	// crab_speed [mm/s]
	crab_speed[next_ind][ind] = sqrt(pow(-neck_velocity[next_ind][ind].x*direction_vector_filtered[next_ind][ind].y, 2) + pow(neck_velocity[next_ind][ind].y*direction_vector_filtered[next_ind][ind].x, 2));

	// v_norm [mm/s]
	v_norm[next_ind][ind] = 1. / 3 * (neck_down_speed[next_ind][ind] + neck_speed[next_ind][ind] + neck_top_speed[next_ind][ind]);
	v_norm[next_ind][ind] = 15.*tanh(v_norm[next_ind][ind] / 15.);

	// v_centroid [mm/s]
	cv::Point2f centroid_speed;
	centroid_speed.x = (centroid[next_ind][ind].x - centroid[next_ind_loc - velocity_step][ind].x) / dt;
	centroid_speed.y = (centroid[next_ind][ind].y - centroid[next_ind_loc - velocity_step][ind].y) / dt;
	v_centroid[next_ind][ind] = sqrt(pow(centroid_speed.x, 2) + pow(centroid_speed.y, 2));

	// speed_reduced [mm/s]
	speed_reduced[next_ind][ind] = tanh(((neck_top_speed[next_ind][ind]) + 0.001) / (3 * v_norm[next_ind][ind] + 0.001));
}


// Calculate the nematic order parameter s of the spine
void giveNematicSSpine(int ind, std::vector<cv::Point2f> spine_loc)
{
	int j, k;
	double cos_theta, norm, dx, dy;

	s[next_ind][ind] = 0.0;

	// iterate over spine segments
	for (int i = 0; i < nSpine - 1; ++i)
	{
		j = (int)floor((double)i*((int)spine_loc.size() - 1) / ((int)nSpine - 1));
		k = (int)floor((double)(i + 1)*((int)spine_loc.size() - 1) / ((int)nSpine - 1));

		dx = spine_loc[k].x - spine_loc[j].x;
		dy = spine_loc[k].y - spine_loc[j].y;

		norm = sqrt(dx * dx + dy * dy);

		dx /= (norm);
		dy /= (norm);

		cos_theta = dx * direction_vector[next_ind][ind].x + dy * direction_vector[next_ind][ind].y;
		s[next_ind][ind] += (3.0*cos_theta*cos_theta - 1.0) / 2.0;
	}

	// normalize
	s[next_ind][ind] /= ((double)nSpine - 1);

	// get index in buffer
	int next_ind_loc;
	if (next_ind == 0)
	{
		next_ind_loc = nBuffers;
	}
	else
	{
		next_ind_loc = next_ind;
	}

	// set to prior value if calculation is not possible
	if (std::isnan(s[next_ind][ind])) {
		s[next_ind][ind] = s[next_ind_loc - 1][ind];
	}
}


// Calculate the reduced eigenvalues of the shape tensor of the contour
void giveEigReduced(int ind, std::vector<cv::Point2f> contour_loc)
{
	double moment_x2 = 0.0, moment_y2 = 0.0, moment_xy = 0.0;

	// calculate moments of the contour
	for (int i = 0; i < contour_loc.size(); ++i)
	{
		moment_x2 += (contour_loc[i].x - neck[next_ind][ind].x) * (contour_loc[i].x - neck[next_ind][ind].x);
		moment_y2 += (contour_loc[i].y - neck[next_ind][ind].y) * (contour_loc[i].y - neck[next_ind][ind].y);
		moment_xy += (contour_loc[i].x - neck[next_ind][ind].x) * (contour_loc[i].y - neck[next_ind][ind].y);
	}

	// normalize moments
	moment_x2 /= (double)contour_loc.size();
	moment_y2 /= (double)contour_loc.size();
	moment_xy /= (double)contour_loc.size();

	// get eig_reduced
	eig_reduced[next_ind][ind] = sqrt(1.0 - 4 * (moment_x2*moment_y2 - moment_xy * moment_xy) / ((moment_x2 + moment_y2) * (moment_x2 + moment_y2)));
}


// Calculate damped distance of larva over time
void giveDampedDistance(int ind)
{
	double gamma_d = 0.9; // smoothing parameter for damped_distance

						  // get index in buffer
	int next_ind_loc;
	if (next_ind == 0)
	{
		next_ind_loc = nBuffers;
	}
	else
	{
		next_ind_loc = next_ind;
	}

	// get recently traveled distance
	double d = sqrt((neck[next_ind][ind].x - neck[next_ind_loc - 1][ind].x) * (neck[next_ind][ind].x - neck[next_ind_loc - 1][ind].x)
		+ (neck[next_ind][ind].y - neck[next_ind_loc - 1][ind].y) * (neck[next_ind][ind].y - neck[next_ind_loc - 1][ind].y));

	// calculate damped_distance as an exponentially smoothed sum of travel distances over time
	damped_distance[next_ind][ind] = d + gamma_d * damped_distance[next_ind_loc - 1][ind];
}


// Calculate features related to direction of movement
void giveMovementDirectionFeatures(int ind)
{
	int next_ind_loc;
	cv::Point2f world;

	// get index in buffer
	if (next_ind == 0)
	{
		next_ind_loc = nBuffers;
	}
	else
	{
		next_ind_loc = next_ind;
	}

	// get parallel_speed
	parallel_speed[next_ind][ind] = neck_velocity_filtered[next_ind_loc - 1][ind].x * direction_vector_filtered[next_ind][ind].x
		+ neck_velocity_filtered[next_ind_loc - 1][ind].y * direction_vector_filtered[next_ind][ind].y;

	// get parallel_speed_tail
	double norm;
	norm = sqrt((tail_velocity_filtered[next_ind_loc - 1][ind].x * tail_velocity_filtered[next_ind_loc - 1][ind].x)
		+ (tail_velocity_filtered[next_ind_loc - 1][ind].y * tail_velocity_filtered[next_ind_loc - 1][ind].y));
	if (norm == 0)
	{
		parallel_speed_tail[next_ind][ind] = 0;
	}
	else
	{
		world.x = tail_velocity_filtered[next_ind_loc - 1][ind].x / norm;
		world.y = tail_velocity_filtered[next_ind_loc - 1][ind].y / norm;
		parallel_speed_tail[next_ind][ind] = world.x * direction_tail_vector_filtered[next_ind][ind].x + world.y * direction_tail_vector_filtered[next_ind][ind].y;
	}

	// get parallel_speed_tail_raw
	parallel_speed_tail_raw[next_ind][ind] = tail_velocity_filtered[next_ind_loc - 1][ind].x * direction_tail_vector_filtered[next_ind][ind].x
		+ tail_velocity_filtered[next_ind_loc - 1][ind].y * direction_tail_vector_filtered[next_ind][ind].y;
}


// Set features to 0 if calculations return a NaN value to avoid problems with temporal smoothing
void correctNanFeatures(int ind)
{
	// get index in buffer
	int next_ind_loc;
	if (next_ind == 0)
	{
		next_ind_loc = nBuffers;
	}
	else
	{
		next_ind_loc = next_ind;
	}

	// current frame
	if (!std::isfinite(v_norm[next_ind][ind])) {
		v_norm[next_ind][ind] = 0;
	}
	if (!std::isfinite(v_centroid[next_ind][ind])) {
		v_centroid[next_ind][ind] = 0;
	}
	if (!std::isfinite(crab_speed[next_ind][ind])) {
		crab_speed[next_ind][ind] = 0;
	}
	if (!std::isfinite(angle_upper_lower[next_ind][ind])) {
		angle_upper_lower[next_ind][ind] = 0;
	}
	if (!std::isfinite(s[next_ind][ind])) {
		s[next_ind][ind] = 0;
	}
	if (!std::isfinite(asymmetry[next_ind][ind])) {
		asymmetry[next_ind][ind] = 0;
	}
	if (!std::isfinite(eig_reduced[next_ind][ind])) {
		eig_reduced[next_ind][ind] = 0;
	}
	if (!std::isfinite(skeleton_length[next_ind][ind])) {
		skeleton_length[next_ind][ind] = 0;
	}
	if (!std::isfinite(perimeter[next_ind][ind])) {
		perimeter[next_ind][ind] = 0;
	}
	if (!std::isfinite(damped_distance[next_ind][ind])) {
		damped_distance[next_ind][ind] = 0;
	}
	if (!std::isfinite(speed_reduced[next_ind][ind])) {
		speed_reduced[next_ind][ind] = 0;
	}
	if (!std::isfinite(parallel_speed[next_ind][ind])) {
		parallel_speed[next_ind][ind] = 0;
	}
	if (!std::isfinite(parallel_speed_tail[next_ind][ind])) {
		parallel_speed_tail[next_ind][ind] = 0;
	}
	if (!std::isfinite(parallel_speed_tail_raw[next_ind][ind])) {
		parallel_speed_tail_raw[next_ind][ind] = 0;
	}
	if (!std::isfinite(direction_vector[next_ind][ind].x)) {
		direction_vector[next_ind][ind].x = 0;
	}
	if (!std::isfinite(direction_vector[next_ind][ind].y)) {
		direction_vector[next_ind][ind].y = 0;
	}
	if (!std::isfinite(direction_tail_vector[next_ind][ind].x)) {
		direction_tail_vector[next_ind][ind].x = 0;
	}
	if (!std::isfinite(direction_tail_vector[next_ind][ind].y)) {
		direction_tail_vector[next_ind][ind].y = 0;
	}
	if (!std::isfinite(direction_head_vector[next_ind][ind].x)) {
		direction_head_vector[next_ind][ind].x = 0;
	}
	if (!std::isfinite(direction_head_vector[next_ind][ind].y)) {
		direction_head_vector[next_ind][ind].y = 0;
	}
	if (!std::isfinite(head_velocity[next_ind][ind].x)) {
		head_velocity[next_ind][ind].x = 0;
	}
	if (!std::isfinite(head_velocity[next_ind][ind].y)) {
		head_velocity[next_ind][ind].y = 0;
	}
	if (!std::isfinite(neck_velocity[next_ind][ind].x)) {
		neck_velocity[next_ind][ind].x = 0;
	}
	if (!std::isfinite(neck_velocity[next_ind][ind].y)) {
		neck_velocity[next_ind][ind].y = 0;
	}
	if (!std::isfinite(tail_velocity[next_ind][ind].x)) {
		tail_velocity[next_ind][ind].x = 0;
	}
	if (!std::isfinite(tail_velocity[next_ind][ind].y)) {
		tail_velocity[next_ind][ind].y = 0;
	}
	if (!std::isfinite(neck_down_velocity[next_ind][ind].x)) {
		neck_down_velocity[next_ind][ind].x = 0;
	}
	if (!std::isfinite(neck_down_velocity[next_ind][ind].y)) {
		neck_down_velocity[next_ind][ind].y = 0;
	}
	if (!std::isfinite(neck_top_velocity[next_ind][ind].x)) {
		neck_top_velocity[next_ind][ind].x = 0;
	}
	if (!std::isfinite(neck_top_velocity[next_ind][ind].y)) {
		neck_top_velocity[next_ind][ind].y = 0;
	}

	// previous frame
	if (!std::isfinite(v_norm[next_ind_loc - 1][ind])) {
		v_norm[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(v_centroid[next_ind_loc - 1][ind])) {
		v_centroid[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(crab_speed[next_ind_loc - 1][ind])) {
		crab_speed[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(angle_upper_lower[next_ind_loc - 1][ind])) {
		angle_upper_lower[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(s[next_ind_loc - 1][ind])) {
		s[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(asymmetry[next_ind_loc - 1][ind])) {
		asymmetry[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(eig_reduced[next_ind_loc - 1][ind])) {
		eig_reduced[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(skeleton_length[next_ind_loc - 1][ind])) {
		skeleton_length[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(perimeter[next_ind_loc - 1][ind])) {
		perimeter[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(damped_distance[next_ind_loc - 1][ind])) {
		damped_distance[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(speed_reduced[next_ind_loc - 1][ind])) {
		speed_reduced[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(parallel_speed[next_ind_loc - 1][ind])) {
		parallel_speed[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(parallel_speed_tail[next_ind_loc - 1][ind])) {
		parallel_speed_tail[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(parallel_speed_tail_raw[next_ind_loc - 1][ind])) {
		parallel_speed_tail_raw[next_ind_loc - 1][ind] = 0;
	}
	if (!std::isfinite(direction_vector[next_ind_loc - 1][ind].x)) {
		direction_vector[next_ind_loc - 1][ind].x = 0;
	}
	if (!std::isfinite(direction_vector[next_ind_loc - 1][ind].y)) {
		direction_vector[next_ind_loc - 1][ind].y = 0;
	}
	if (!std::isfinite(direction_tail_vector[next_ind_loc - 1][ind].x)) {
		direction_tail_vector[next_ind_loc - 1][ind].x = 0;
	}
	if (!std::isfinite(direction_tail_vector[next_ind_loc - 1][ind].y)) {
		direction_tail_vector[next_ind_loc - 1][ind].y = 0;
	}
	if (!std::isfinite(direction_head_vector[next_ind_loc - 1][ind].x)) {
		direction_head_vector[next_ind_loc - 1][ind].x = 0;
	}
	if (!std::isfinite(direction_head_vector[next_ind_loc - 1][ind].y)) {
		direction_head_vector[next_ind_loc - 1][ind].y = 0;
	}
	if (!std::isfinite(head_velocity[next_ind_loc - 1][ind].x)) {
		head_velocity[next_ind_loc - 1][ind].x = 0;
	}
	if (!std::isfinite(head_velocity[next_ind_loc - 1][ind].y)) {
		head_velocity[next_ind_loc - 1][ind].y = 0;
	}
	if (!std::isfinite(neck_velocity[next_ind_loc - 1][ind].x)) {
		neck_velocity[next_ind_loc - 1][ind].x = 0;
	}
	if (!std::isfinite(neck_velocity[next_ind_loc - 1][ind].y)) {
		neck_velocity[next_ind_loc - 1][ind].y = 0;
	}
	if (!std::isfinite(tail_velocity[next_ind_loc - 1][ind].x)) {
		tail_velocity[next_ind_loc - 1][ind].x = 0;
	}
	if (!std::isfinite(tail_velocity[next_ind_loc - 1][ind].y)) {
		tail_velocity[next_ind_loc - 1][ind].y = 0;
	}
	if (!std::isfinite(neck_down_velocity[next_ind_loc - 1][ind].x)) {
		neck_down_velocity[next_ind_loc - 1][ind].x = 0;
	}
	if (!std::isfinite(neck_down_velocity[next_ind_loc - 1][ind].y)) {
		neck_down_velocity[next_ind_loc - 1][ind].y = 0;
	}
	if (!std::isfinite(neck_top_velocity[next_ind_loc - 1][ind].x)) {
		neck_top_velocity[next_ind_loc - 1][ind].x = 0;
	}
	if (!std::isfinite(neck_top_velocity[next_ind_loc - 1][ind].y)) {
		neck_top_velocity[next_ind_loc - 1][ind].y = 0;
	}
}


// Exponentially smooth features
void smoothValues(int ind)
{
	// get index in buffer
	int next_ind_loc;
	if (next_ind == 0)
	{
		next_ind_loc = nBuffers;
	}
	else
	{
		next_ind_loc = next_ind;
	}

	// smoothing parameters
	double sigma_loc = 5.0;
	double tau = 0.25;
	double tau_long_time = 5.0;
	double dt_eff = time_stamp[next_ind] - time_stamp[next_ind_loc - 1];
	double alpha = dt_eff / tau;
	double alpha_long_time = dt_eff / tau_long_time;

	// smooth features
	v_norm_filtered[next_ind][ind] = (1.0 - alpha) * v_norm_filtered[next_ind_loc - 1][ind] + alpha * sigma_loc * tanh(v_norm[next_ind][ind] / sigma_loc); // tanh prevents v_norm_filtered from becoming too large if v_norm is large in this frame
	v_centroid_filtered[next_ind][ind] = (1.0 - alpha) * v_centroid_filtered[next_ind_loc - 1][ind] + alpha * v_centroid[next_ind][ind];
	crab_speed_filtered[next_ind][ind] = (1.0 - alpha) * crab_speed_filtered[next_ind_loc - 1][ind] + alpha * crab_speed[next_ind][ind];
	angle_upper_lower_filtered[next_ind][ind] = (1.0 - alpha) * angle_upper_lower_filtered[next_ind_loc - 1][ind] + alpha * angle_upper_lower[next_ind][ind];
	s_filtered[next_ind][ind] = (1.0 - alpha) * s_filtered[next_ind_loc - 1][ind] + alpha * s[next_ind][ind];
	asymmetry_filtered[next_ind][ind] = (1.0 - alpha) * asymmetry_filtered[next_ind_loc - 1][ind] + alpha * asymmetry[next_ind][ind];
	eig_reduced_filtered[next_ind][ind] = (1.0 - alpha) * eig_reduced_filtered[next_ind_loc - 1][ind] + alpha * eig_reduced[next_ind][ind];
	skeleton_length_filtered[next_ind][ind] = (1.0 - alpha) * skeleton_length_filtered[next_ind_loc - 1][ind] + alpha * skeleton_length[next_ind][ind];
	perimeter_filtered[next_ind][ind] = (1.0 - alpha) * perimeter_filtered[next_ind_loc - 1][ind] + alpha * perimeter[next_ind][ind];
	damped_distance_filtered[next_ind][ind] = (1.0 - alpha) * damped_distance_filtered[next_ind_loc - 1][ind] + alpha * damped_distance[next_ind][ind];
	speed_reduced_filtered[next_ind][ind] = (1.0 - alpha) * speed_reduced_filtered[next_ind_loc - 1][ind] + alpha * speed_reduced[next_ind][ind];
	parallel_speed_filtered[next_ind][ind] = (1.0 - alpha) * parallel_speed_filtered[next_ind_loc - 1][ind] + alpha * parallel_speed[next_ind][ind];
	parallel_speed_tail_filtered[next_ind][ind] = (1.0 - alpha) * parallel_speed_tail_filtered[next_ind_loc - 1][ind] + alpha * parallel_speed_tail[next_ind][ind];
	parallel_speed_tail_raw_filtered[next_ind][ind] = (1.0 - alpha) * parallel_speed_tail_raw_filtered[next_ind_loc - 1][ind] + alpha * parallel_speed_tail_raw[next_ind][ind];
	direction_vector_filtered[next_ind][ind].x = (1.0 - alpha) * direction_vector_filtered[next_ind_loc - 1][ind].x + alpha * direction_vector[next_ind][ind].x;
	direction_vector_filtered[next_ind][ind].y = (1.0 - alpha) * direction_vector_filtered[next_ind_loc - 1][ind].y + alpha * direction_vector[next_ind][ind].y;
	direction_tail_vector_filtered[next_ind][ind].x = (1.0 - alpha) * direction_tail_vector_filtered[next_ind_loc - 1][ind].x + alpha * direction_tail_vector[next_ind][ind].x;
	direction_tail_vector_filtered[next_ind][ind].y = (1.0 - alpha) * direction_tail_vector_filtered[next_ind_loc - 1][ind].y + alpha * direction_tail_vector[next_ind][ind].y;
	direction_head_vector_filtered[next_ind][ind].x = (1.0 - alpha) * direction_head_vector_filtered[next_ind_loc - 1][ind].x + alpha * direction_head_vector[next_ind][ind].x;
	direction_head_vector_filtered[next_ind][ind].y = (1.0 - alpha) * direction_head_vector_filtered[next_ind_loc - 1][ind].y + alpha * direction_head_vector[next_ind][ind].y;

	// smooth velocity features
	head_velocity_filtered[next_ind][ind].x = (1.0 - alpha) * head_velocity_filtered[next_ind_loc - 1][ind].x + alpha * head_velocity[next_ind][ind].x;
	head_velocity_filtered[next_ind][ind].y = (1.0 - alpha) * head_velocity_filtered[next_ind_loc - 1][ind].y + alpha * head_velocity[next_ind][ind].y;
	neck_velocity_filtered[next_ind][ind].x = (1.0 - alpha) * neck_velocity_filtered[next_ind_loc - 1][ind].x + alpha * neck_velocity[next_ind][ind].x;
	neck_velocity_filtered[next_ind][ind].y = (1.0 - alpha) * neck_velocity_filtered[next_ind_loc - 1][ind].y + alpha * neck_velocity[next_ind][ind].y;
	tail_velocity_filtered[next_ind][ind].x = (1.0 - alpha) * tail_velocity_filtered[next_ind_loc - 1][ind].x + alpha * tail_velocity[next_ind][ind].x;
	tail_velocity_filtered[next_ind][ind].y = (1.0 - alpha) * tail_velocity_filtered[next_ind_loc - 1][ind].y + alpha * tail_velocity[next_ind][ind].y;
	neck_down_velocity_filtered[next_ind][ind].x = (1.0 - alpha) * neck_down_velocity_filtered[next_ind_loc - 1][ind].x + alpha * neck_down_velocity[next_ind][ind].x;
	neck_down_velocity_filtered[next_ind][ind].y = (1.0 - alpha) * neck_down_velocity_filtered[next_ind_loc - 1][ind].y + alpha * neck_down_velocity[next_ind][ind].y;
	neck_top_velocity_filtered[next_ind][ind].x = (1.0 - alpha) * neck_top_velocity_filtered[next_ind_loc - 1][ind].x + alpha * neck_top_velocity[next_ind][ind].x;
	neck_top_velocity_filtered[next_ind][ind].y = (1.0 - alpha) * neck_top_velocity_filtered[next_ind_loc - 1][ind].y + alpha * neck_top_velocity[next_ind][ind].y;

	// smooth velocity features over a longer time interval using a smaller value for alpha
	v_norm_long_time[next_ind][ind] = (1.0 - alpha_long_time) * v_norm_long_time[next_ind_loc - 1][ind] + alpha_long_time * v_norm[next_ind][ind];
	v_centroid_long_time[next_ind][ind] = (1.0 - alpha_long_time) * v_centroid_long_time[next_ind_loc - 1][ind] + alpha_long_time * v_centroid[next_ind][ind];
}


// Uses a convolution algorithm adapted from Masson et al. 2012 to approximate a smoothed squared derivative for each feature
void convolveToDifferentiate(int ind)
{
	double amplitude;

	// get index in buffer
	int next_ind_loc;
	if (next_ind == 0)
	{
		next_ind_loc = nBuffers;
	}
	else
	{
		next_ind_loc = next_ind;
	}

	// convolution parameters
	double tau = 0.25;
	double dt_eff = time_stamp[next_ind] - time_stamp[next_ind_loc - 1];
	double lambda = 1 / tau;
	double lambda_eff = lambda * dt_eff;

	crab_speed1[next_ind][ind] = (1.0 - lambda_eff) * crab_speed1[next_ind_loc - 1][ind] + (0.5)*dt_eff*(crab_speed[next_ind_loc - 1][ind] + crab_speed[next_ind][ind]);
	crab_speed2[next_ind][ind] = lambda_eff * crab_speed1[next_ind_loc - 1][ind] + (1.0 - lambda_eff) * crab_speed2[next_ind][ind];
	crab_speed_convolved_squared[next_ind][ind] = 500.0*pow(crab_speed1[next_ind][ind] - crab_speed2[next_ind][ind], 2);

	angle_upper_lower1[next_ind][ind] = (1.0 - lambda_eff) * angle_upper_lower1[next_ind_loc - 1][ind] + (0.5)*dt_eff*(angle_upper_lower[next_ind_loc - 1][ind] + angle_upper_lower[next_ind][ind]);
	angle_upper_lower2[next_ind][ind] = lambda_eff * angle_upper_lower1[next_ind_loc - 1][ind] + (1.0 - lambda_eff) * angle_upper_lower2[next_ind][ind];
	angle_upper_lower_convolved_squared[next_ind][ind] = 1000.0*pow(angle_upper_lower1[next_ind][ind] - angle_upper_lower2[next_ind][ind], 2);

	s1[next_ind][ind] = (1.0 - lambda_eff) * s1[next_ind_loc - 1][ind] + (0.5)*dt_eff*(s[next_ind_loc - 1][ind] + s[next_ind][ind]);
	s2[next_ind][ind] = lambda_eff * s1[next_ind_loc - 1][ind] + (1.0 - lambda_eff) * s2[next_ind][ind];
	s_convolved_squared[next_ind][ind] = 1000.0*pow(s1[next_ind][ind] - s2[next_ind][ind], 2);

	v_norm1[next_ind][ind] = (1.0 - lambda_eff) * v_norm1[next_ind_loc - 1][ind] + (0.5)*dt_eff*(v_norm[next_ind_loc - 1][ind] + v_norm[next_ind][ind]);
	v_norm2[next_ind][ind] = lambda_eff * v_norm1[next_ind_loc - 1][ind] + pow(s1[next_ind][ind] - s2[next_ind][ind], 2);
	v_norm_convolved_squared[next_ind][ind] = 50.0*pow(v_norm1[next_ind][ind] - v_norm2[next_ind][ind], 2);

	asymmetry1[next_ind][ind] = (1.0 - lambda_eff) * asymmetry1[next_ind_loc - 1][ind] + (0.5)*dt_eff*(asymmetry[next_ind_loc - 1][ind] + asymmetry[next_ind][ind]);
	asymmetry2[next_ind][ind] = lambda_eff * asymmetry1[next_ind_loc - 1][ind] + (1.0 - lambda_eff) * asymmetry2[next_ind][ind];
	asymmetry_convolved_squared[next_ind][ind] = 1000.0*pow(asymmetry1[next_ind][ind] - asymmetry2[next_ind][ind], 2);

	eig_reduced1[next_ind][ind] = (1.0 - lambda_eff) * eig_reduced1[next_ind_loc - 1][ind] + (0.5)*dt_eff*(eig_reduced[next_ind_loc - 1][ind] + eig_reduced[next_ind][ind]);
	eig_reduced2[next_ind][ind] = lambda_eff * eig_reduced1[next_ind_loc - 1][ind] + (1.0 - lambda_eff) * eig_reduced2[next_ind][ind];
	eig_reduced_convolved_squared[next_ind][ind] = 100000.0*pow(eig_reduced1[next_ind][ind] - eig_reduced2[next_ind][ind], 2);

	skeleton_length1[next_ind][ind] = (1.0 - lambda_eff) * skeleton_length1[next_ind_loc - 1][ind] + (0.5)*dt_eff*(skeleton_length[next_ind_loc - 1][ind] + skeleton_length[next_ind][ind]);
	skeleton_length2[next_ind][ind] = lambda_eff * skeleton_length1[next_ind_loc - 1][ind] + (1.0 - lambda_eff) * skeleton_length2[next_ind][ind];
	skeleton_length_convolved_squared[next_ind][ind] = 1000.0*pow(skeleton_length1[next_ind][ind] - skeleton_length2[next_ind][ind], 2);

	perimeter1[next_ind][ind] = (1.0 - lambda_eff) * perimeter1[next_ind_loc - 1][ind] + (0.5)*dt_eff*(perimeter[next_ind_loc - 1][ind] + perimeter[next_ind][ind]);
	perimeter2[next_ind][ind] = lambda_eff * perimeter1[next_ind_loc - 1][ind] + (1.0 - lambda_eff) * perimeter2[next_ind][ind];
	perimeter_convolved_squared[next_ind][ind] = 1000.0*pow(perimeter1[next_ind][ind] - perimeter2[next_ind][ind], 2);

	damped_distance1[next_ind][ind] = (1.0 - lambda_eff) * damped_distance1[next_ind_loc - 1][ind] + (0.5)*dt_eff*(damped_distance[next_ind_loc - 1][ind] + damped_distance[next_ind][ind]);
	damped_distance2[next_ind][ind] = lambda_eff * damped_distance1[next_ind_loc - 1][ind] + (1.0 - lambda_eff) * damped_distance2[next_ind][ind];
	damped_distance_convolved_squared[next_ind][ind] = 1000.0*pow(damped_distance1[next_ind][ind] - damped_distance2[next_ind][ind], 2);

	speed_reduced1[next_ind][ind] = (1.0 - lambda_eff) * speed_reduced1[next_ind_loc - 1][ind] + (0.5)*dt_eff*(speed_reduced[next_ind_loc - 1][ind] + speed_reduced[next_ind][ind]);
	speed_reduced2[next_ind][ind] = lambda_eff * speed_reduced1[next_ind_loc - 1][ind] + (1.0 - lambda_eff) * speed_reduced2[next_ind][ind];
	speed_reduced_convolved_squared[next_ind][ind] = 1000.0*pow(speed_reduced1[next_ind][ind] - speed_reduced2[next_ind][ind], 2);

	parallel_speed1[next_ind][ind] = (1.0 - lambda_eff) * parallel_speed1[next_ind_loc - 1][ind] + (0.5)*dt_eff*(parallel_speed[next_ind_loc - 1][ind] + parallel_speed[next_ind][ind]);
	parallel_speed2[next_ind][ind] = lambda_eff * parallel_speed1[next_ind_loc - 1][ind] + (1.0 - lambda_eff) * parallel_speed2[next_ind][ind];
	parallel_speed_convolved_squared[next_ind][ind] = 1000.0*pow(parallel_speed1[next_ind][ind] - parallel_speed2[next_ind][ind], 2);

	parallel_speed_tail1[next_ind][ind] = (1.0 - lambda_eff) * parallel_speed_tail1[next_ind_loc - 1][ind] + (0.5)*dt_eff*(parallel_speed_tail[next_ind_loc - 1][ind] + parallel_speed_tail[next_ind][ind]);
	parallel_speed_tail2[next_ind][ind] = lambda_eff * parallel_speed_tail1[next_ind_loc - 1][ind] + (1.0 - lambda_eff) * parallel_speed_tail2[next_ind][ind];
	parallel_speed_tail_convolved_squared[next_ind][ind] = 1000.0*pow(parallel_speed_tail1[next_ind][ind] - parallel_speed_tail2[next_ind][ind], 2);
}


// Bend classification
void isBend(int ind)
{
	if (s_filtered[next_ind][ind] < 0.85
		&& eig_reduced_filtered[next_ind][ind] < 0.85
		&& angle_upper_lower_filtered[next_ind][ind] > 0.4)
	{
		bend[next_ind][ind] = true;
		d_bend[next_ind][ind] = 1;
	}
	else
	{
		bend[next_ind][ind] = false;
		d_bend[next_ind][ind] = 0;
	}
}


// Left and right classification
void isLeftRight(int ind)
{
	if (angle_upper_lower_filtered[next_ind][ind] > 0.4) {
		if (asymmetry[next_ind][ind] >= 0.4) {
			left[next_ind][ind] = true;
			right[next_ind][ind] = false;
			//d_left[next_ind][ind] = 1;
			//d_right[next_ind][ind] = 0;
		}
		else if (asymmetry[next_ind][ind] <= -0.4) {
			left[next_ind][ind] = false;
			right[next_ind][ind] = true;
			//d_left[next_ind][ind] = 0;
			//d_right[next_ind][ind] = 1;
		}
		else {
			left[next_ind][ind] = false;
			right[next_ind][ind] = false;
			//d_left[next_ind][ind] = 0;
			//d_right[next_ind][ind] = 0;
		}
	}
	else {
		left[next_ind][ind] = false;
		right[next_ind][ind] = false;
		//d_left[next_ind][ind] = 0;
		//d_right[next_ind][ind] = 0;
	}
}


// Roll classification
void isRoll(int ind)
{
	if ((angle_upper_lower_filtered[next_ind][ind] > 1.0)
		&& (angle_upper_lower_filtered[next_ind][ind] < 1.8)
		&& (crab_speed_filtered[next_ind][ind] > 1.0)
		&& (damped_distance_filtered[next_ind][ind] > 0.64)
		&& (v_norm_filtered[next_ind][ind] > 1.2)
		&& ((asymmetry_filtered[next_ind][ind] > 0.65) || (asymmetry_filtered[next_ind][ind] < -0.65))
		&& (s[next_ind][ind] < 0.8)
		&& (s_filtered[next_ind][ind] > 0.2)
		&& (eig_reduced_filtered[next_ind][ind] < 0.7)
		&& (eig_reduced_filtered[next_ind][ind] > -1.5 * s_filtered[next_ind][ind] + 0.45)
		&& (v_norm_long_time[next_ind][ind] > 0.5)
		&& (parallel_speed_tail_filtered[next_ind][ind] > -0.6)
		&& ((parallel_speed_tail_filtered[next_ind][ind] > -0.4) || (asymmetry_convolved_squared[next_ind][ind] > 10))
		&& (speed_reduced_filtered[next_ind][ind] < 0.38)
		&& (ball[next_ind][ind] == false)
		) {
		roll[next_ind][ind] = true;
		time_stamp_last_roll[ind] = time_stamp[next_ind];
		d_roll[next_ind][ind] = 1;
	}
	else {
		roll[next_ind][ind] = false;
		d_roll[next_ind][ind] = 0;
	}
}


// Forward classification
void isForward(int ind)
{
	if ((parallel_speed_tail_filtered[next_ind][ind] > 0.6)
		&& (parallel_speed_tail_raw_filtered[next_ind][ind] > 0.6)
		&& (weird[next_ind][ind] == false))
	{
		forward[next_ind][ind] = true;
	}
	else
	{
		forward[next_ind][ind] = false;
	}
}


// Forward peristaltic wave classification
void isForwardPeristaltic(int ind)
{
	if ((parallel_speed_tail_filtered[next_ind][ind] > 0.6)
		&& (parallel_speed_tail_raw_filtered[next_ind][ind] > 0.6)
		&& (parallel_speed_tail_raw[next_ind][ind] > 0.8)
		&& (weird[next_ind][ind] == false))
	{
		forward_peristaltic[next_ind][ind] = true;
	}
	else
	{
		forward_peristaltic[next_ind][ind] = false;
	}
}


// Ball classifier
void isBall(int ind)
{
	//  features used for training:
	//	0	 eig_reduced
	//  1	 larva_arc_ratio
	//	2	 larva_area_ratio

	std::vector<double> variables_ball;
	int n_variables = 3; // number of features used for training
	variables_ball.resize(n_variables);

	// define inputs to the neural network
	variables_ball[0] = eig_reduced[next_ind][ind];
	variables_ball[1] = larva_arc_ratio[next_ind][ind];
	variables_ball[2] = larva_area_ratio[next_ind][ind];

	// initialize neural network parameters
	std::vector<double> w1_ball_;
	std::vector<double> b1_ball_;
	std::vector<double> w2_ball_;
	std::vector<double> b2_ball_;
	std::vector<double> range_input_ball_;

	// layer 1
	int n_layer1 = 5;

	w1_ball_.resize(n_layer1*n_variables);
	// neuron 1:
	w1_ball_[0] = -2.164907;
	w1_ball_[1] = 0.329330;
	w1_ball_[2] = -3.751981;

	// neuron 2:
	w1_ball_[3] = -0.109675;
	w1_ball_[4] = -4.709132;
	w1_ball_[5] = 4.538956;

	// neuron 3:
	w1_ball_[6] = -6.947161;
	w1_ball_[7] = -0.034429;
	w1_ball_[8] = 8.383541;

	// neuron 4:
	w1_ball_[9] = 11.856362;
	w1_ball_[10] = -6.054295;
	w1_ball_[11] = -3.265648;

	// neuron 5:
	w1_ball_[12] = 0.819264;
	w1_ball_[13] = 3.434949;
	w1_ball_[14] = -8.662766;

	b1_ball_.resize(n_layer1);
	// neuron 1:
	b1_ball_[0] = 0.819594;

	// neuron 2:
	b1_ball_[1] = 2.601275;

	// neuron 3:
	b1_ball_[2] = -4.947874;

	// neuron 4:
	b1_ball_[3] = -6.507559;

	// neuron 5:
	b1_ball_[4] = -6.424620;

	// layer 2
	int n_layer2 = 1;

	w2_ball_.resize(n_layer1*n_layer2);
	// neuron 1:
	w2_ball_[0] = -8.020819;
	w2_ball_[1] = -4.327108;
	w2_ball_[2] = 4.092607;
	w2_ball_[3] = -9.870084;
	w2_ball_[4] = 13.164315;

	b2_ball_.resize(n_layer2);
	// neuron 1:
	b2_ball_[0] = 12.871267;

	// input Range
	range_input_ball_.resize(2 * n_variables);
	range_input_ball_[0] = 0.011654;
	range_input_ball_[1] = 0.884381;
	range_input_ball_[2] = 1.039880;
	range_input_ball_[3] = 1.504890;
	range_input_ball_[4] = 0.557688;
	range_input_ball_[5] = 1.010910;

	double thresh_ball_ = 0.8;

	// normalize variables
	int ind_loc = 0;
	for (int i = 0; i<n_variables; ++i)
	{
		variables_ball[i] = 2.0*(variables_ball[i] - range_input_ball_[ind_loc]) / (range_input_ball_[ind_loc + 1] - range_input_ball_[ind_loc]) - 1;
		ind_loc += 2;
	}

	std::vector<double> inter_ball;
	inter_ball.resize(n_layer1);

	// calculate the sum of the weighted inputs plus the bias in the hidden layer
	ind_loc = 0;
	for (int i = 0; i<n_layer1; ++i)
	{
		inter_ball[i] = 0.0;
		for (int j = 0; j<n_variables; ++j)
		{
			inter_ball[i] += w1_ball_[ind_loc] * variables_ball[j];
			ind_loc++;
		}
		inter_ball[i] += b1_ball_[i];

		// pass through the transfer function
		inter_ball[i] = tanh(inter_ball[i]);
	}

	// calculate the sum of the weighted inputs plus the bias in the output layer
	double output = 0;
	for (int i = 0; i<n_layer1; ++i)
	{
		output += w2_ball_[i] * inter_ball[i];
	}
	output += b2_ball_[0];

	// pass through the transfer function
	output = 1 / (1 + exp(-output));

	// classification based on continuous output of neural network
	if (output > thresh_ball_)
	{
		ball[next_ind][ind] = true;

		// reset head tail detection votes
		votes_correct[ind] = 0;
		votes_flipped[ind] = 0;
		time_stamp_last_ball[ind] = time_stamp[next_ind];
	}
	else
	{
		ball[next_ind][ind] = false;
	}
}


// Weird classifier (after detection of a ball, behavior detection has to be modified to account for stabilization period of head tail detection)
void isWeird(int ind)
{
	double thresh_weird_ = 1.5;

	// if there was a ball in the past thresh_weird_ seconds
	if (time_stamp[next_ind] - time_stamp_last_ball[ind] < thresh_weird_) {
		weird[next_ind][ind] = true;
	}
	else {
		weird[next_ind][ind] = false;
	}
}


// Roll Weird classifier (after detection of a roll, behavior detection has to be modified to account for stabilization period of head tail detection)
void isRollWeird(int ind)
{
	double thresh_roll_weird_ = 1.5;

	// if there was a roll in the past thresh_roll_weird_ seconds
	if (time_stamp[next_ind] - time_stamp_last_roll[ind] < thresh_roll_weird_) {
		roll_weird[next_ind][ind] = true;
		back[next_ind][ind] = false;
		forward[next_ind][ind] = false;
		forward_peristaltic[next_ind][ind] = false;
	}
	else {
		roll_weird[next_ind][ind] = false;
	}
}


// Back-up classifier
void isBack(int ind)
{
	if ((parallel_speed_tail_filtered[next_ind][ind] < -0.6)
		&& (parallel_speed_tail_raw_filtered[next_ind][ind] < -0.45)
		&& (weird[next_ind][ind] == false)
		&& (roll_weird[next_ind][ind] == false))
	{
		back[next_ind][ind] = true;
	}
	else
	{
		back[next_ind][ind] = false;
	}
}


// Smooth some behavior classifiers over time
void smoothBehaviors(int ind)
{
	// get index in buffer
	int next_ind_loc;
	if (next_ind == 0)
	{
		next_ind_loc = nBuffers;
	}
	else
	{
		next_ind_loc = next_ind;
	}

	// get time interval for smoothing
	double dt_eff = time_stamp[next_ind] - time_stamp[next_ind_loc - 1];

	// roll
	double thresh_roll_smooth_ = 0.6;
	double tau_roll = 0.1;
	double alpha_roll = dt_eff / tau_roll;
	d_roll_smooth[next_ind][ind] = (1.0 - alpha_roll) * d_roll_smooth[next_ind_loc - 1][ind] + alpha_roll * d_roll[next_ind][ind];
	if (d_roll_smooth[next_ind][ind] > thresh_roll_smooth_) {
		roll_smooth[next_ind][ind] = true;
		back[next_ind][ind] = false;
	}
	else {
		roll_smooth[next_ind][ind] = false;
	}

	//// left and right
	//double thresh_left_right_smooth_ = 0.7;
	//double tau_left_right = 0.05;
	//double alpha_left_right = dt_eff / tau_left_right;
	//d_left_smooth[next_ind][ind] = (1.0 - alpha_left_right) * d_left_smooth[next_ind_loc - 1][ind] + alpha_left_right * d_left[next_ind][ind];
	//d_right_smooth[next_ind][ind] = (1.0 - alpha_left_right) * d_right_smooth[next_ind_loc - 1][ind] + alpha_left_right * d_right[next_ind][ind];
	//if (d_left_smooth[next_ind][ind] > thresh_left_right_smooth_) {
	//	left_smooth[next_ind][ind] = true;
	//}
	//else {
	//	left_smooth[next_ind][ind] = false;
	//}
	//if (d_right_smooth[next_ind][ind] > thresh_left_right_smooth_) {
	//	right_smooth[next_ind][ind] = true;
	//}
	//else {
	//	right_smooth[next_ind][ind] = false;
	//}
	//if (left_smooth[next_ind][ind] && right_smooth[next_ind][ind]) {
	//	left_smooth[next_ind][ind] = false;
	//	right_smooth[next_ind][ind] = false;
	//}

	// bend
	double thresh_bend_smooth_ = 0.7;
	double tau_bend = 0.06;
	double alpha_bend = dt_eff / tau_bend;
	d_bend_smooth[next_ind][ind] = (1.0 - alpha_bend) * d_bend_smooth[next_ind_loc - 1][ind] + alpha_bend * d_bend[next_ind][ind];
	if (d_bend_smooth[next_ind][ind] > thresh_bend_smooth_) {
		bend_smooth[next_ind][ind] = true;
	}
	else {
		bend_smooth[next_ind][ind] = false;
	}
}


// Correct detection of left and right during weird state
void correctLeftRight(int ind)
{
	if (weird[next_ind][ind])
	{
		// keep last bend direction
		if (last_bend_direction[ind])
		{
			left[next_ind][ind] = true;
			right[next_ind][ind] = false;
		}
		else
		{
			left[next_ind][ind] = false;
			right[next_ind][ind] = true;
		}
	}
	else
	{
		// assign last bend direction
		if (left[next_ind][ind])
		{
			last_bend_direction[ind] = true;
		}
		if (right[next_ind][ind])
		{
			last_bend_direction[ind] = false;
		}
	}
}


// Behavior detection for a single larva
void behaviorDetectionSingleLarva(std::vector<std::vector<cv::Point2f> > contour, std::vector<std::vector<cv::Point2f> > & hull, std::vector<bool> larvaeValid, int ind)
{
	if (larvaeValid[ind]) {
		// all behavior detection assumes that spatial calculations are in mm

		// feature extraction
		getLandmarkPoints(ind, spine_smooth[ind], contour[ind]);
		getContourHullRatios(ind, contour[ind], hull[ind]);
		giveDirectionVectorsAndAngles(ind);
		calcVelocities(ind);
		giveNematicSSpine(ind, spine_smooth[ind]);
		giveEigReduced(ind, contour[ind]);
		giveDampedDistance(ind);
		giveMovementDirectionFeatures(ind);
		correctNanFeatures(ind);
		smoothValues(ind);
		convolveToDifferentiate(ind);

		// behavior classification
		isLeftRight(ind);
		isBend(ind);
		isBall(ind);
		isWeird(ind);
		isBack(ind);
		isForward(ind);
		isForwardPeristaltic(ind);
		isRoll(ind);
		isRollWeird(ind);

		// smooth behaviors
		smoothBehaviors(ind);

		// correct left and right detection
		correctLeftRight(ind);
	}
	else {
		// if larva is invalid in this frame, use the last valid data for this larva
		lockAllFeatures(ind);
	}
}


// Behavior detection parent function
void behaviorDetection(std::vector<std::vector<cv::Point2f> > contour, std::vector<std::vector<cv::Point2f> > & hull, std::vector<int> larvaeIDs, std::vector<bool> larvaeValid)
{
	// for all larvae
	for (int ind = 0; ind < larvaeIDs.size(); ++ind) {

		// run behavior detection for each larva
		behaviorDetectionSingleLarva(contour, hull, larvaeValid, ind);
	}
}


// Check if point p lies on the segment from p1 to p2
bool pointOnSegment(cv::Point2f p1, cv::Point2f p2, cv::Point2f p)
{
	if (p == p1 || p == p2) {
		// case in which p equals either p1 or p2
		return true;
	}
	else if (p1.x == p2.x) {
		// if the line is vertical (necessary to avoid division by 0)
		return (p.x == p1.x && ((p.y > p1.y && p.y < p2.y) || (p.y < p1.y && p.y > p2.y)));
	}
	else {
		// if coordinates are in the right range
		if (((p.x > p1.x && p.x < p2.x) || (p.x < p1.x && p.x > p2.x))
			&& ((p.y > p1.y && p.y < p2.y) || (p.y < p1.y && p.y > p2.y))) {
			// compare slopes
			return ((p.y - p1.y) / (p.x - p1.x)) == ((p2.y - p1.y) / (p2.x - p1.x));
		}
		else {
			return false;
		}
	}
}


// Check if two lines are parallel
bool isParallel(cv::Point2f start1, cv::Point2f end1, cv::Point2f start2, cv::Point2f end2) {
	// if slope is not well-defined for at least one line, check the other
	if ((start1.x == end1.x) || (start2.x == end2.x)) {
		return (start1.x == end1.x) && (start2.x == end2.x);
	}
	// otherwise compare slopes
	else {
		return ((end1.y - start1.y) / (end1.x - start1.x)) == ((end2.y - start2.y) / (end2.x - start2.x));
	}
}


// Check if two segments are intersecting or touching (except the case where the only way they touch is that they share an endpoint)
bool segmentsIntersect(cv::Point2f start1, cv::Point2f end1, cv::Point2f start2, cv::Point2f end2)
{
	if (start1 == end2) {
		return (pointOnSegment(start1, end1, start2) || pointOnSegment(start2, end2, end1));
	}
	else if (end1 == start2) {
		return (pointOnSegment(start1, end1, end2) || pointOnSegment(start2, end2, start1));
	}
	else {
		// if the segments are parallel, it is sufficient to check if one endpoint lies on the other segment
		if (isParallel(start1, end1, start2, end2)) {
			return (pointOnSegment(start1, end1, start2) || pointOnSegment(start1, end1, end2) || pointOnSegment(start2, end2, start1) || pointOnSegment(start2, end2, end1));
		}
		// otherwise find intersection of the two lines and check if it lies on both segments
		else {
			double m1, m2, b1, b2;
			cv::Point2f intersection;
			// treat cases with invalid slope
			if (start1.x == end1.x) {
				m2 = (end2.y - start2.y) / (end2.x - start2.x);
				b2 = end2.y - m2 * end2.x;
				intersection.x = start1.x;
				intersection.y = m2 * intersection.x + b2;
			}
			else if (start2.x == end2.x) {
				m1 = (end1.y - start1.y) / (end1.x - start1.x);
				b1 = end1.y - m1 * end1.x;
				intersection.x = start2.x;
				intersection.y = m1 * intersection.x + b1;
			}
			else {
				m1 = (end1.y - start1.y) / (end1.x - start1.x);
				m2 = (end2.y - start2.y) / (end2.x - start2.x);
				b1 = end1.y - m1 * end1.x;
				b2 = end2.y - m2 * end2.x;
				intersection.x = (b2 - b1) / (m1 - m2);
				intersection.y = m1 * intersection.x + b1;
			}
			return (pointOnSegment(start1, end1, intersection) && pointOnSegment(start2, end2, intersection));
		}
	}
}


// Check if contour is self-intersecting
// adapted code from http://tutorialspots.com/php-how-to-check-if-a-polygon-is-self-intersect-2-1023.html
bool isSelfIntersect(std::vector<cv::Point2f> polygon)
{
	for (int i = 0; i < polygon.size() - 1; ++i)
	{
		for (int h = i + 1; h < polygon.size(); ++h)
		{
			// Do two vertices lie on top of one another?
			if (polygon[i] == polygon[h])
			{
				return true;
			}

			// check if edges intersect
			if (segmentsIntersect(polygon[i], polygon[(i + 1) % polygon.size()], polygon[h], polygon[(h + 1) % polygon.size()])) {
				return true;
			}
		}
	}
	return false;
}


// Excludes invalid larvae // REVIEW: we have to test whether this function is working
void excludeInvalidLarvae(std::vector<std::vector<cv::Point2f> > contour, std::vector<std::vector<cv::Point2f> > spine, std::vector<int> larvaeIDs, std::vector<bool> larvaeValid)
{
	double perimeter_temp;
	double skeleton_length_temp;
	for (int ind = 0; ind < larvaeIDs.size(); ++ind) {

		// calculate temporary perimeter and spine length
		perimeter_temp = getArcLength(contour[ind]);
		skeleton_length_temp = 0;
		for (int i = 1; i < nSpine; ++i) {
			skeleton_length_temp += sqrt((spine[ind][i].x - spine[ind][i - 1].x) * (spine[ind][i].x - spine[ind][i - 1].x)
				+ (spine[ind][i].y - spine[ind][i - 1].y) * (spine[ind][i].y - spine[ind][i - 1].y));
		}

		// calculate centroid
		cv::Point2f centroid_temp;
		centroid_temp.x = 0;
		centroid_temp.y = 0;
		for (int i = 0; i < contour[ind].size(); ++i) {
			centroid_temp += contour[ind][i];
		}
		centroid_temp /= double(contour[ind].size());
		double travel_distance = sqrt(pow(centroid_temp.x - centroid[prev_ind][ind].x, 2) + pow(centroid_temp.y - centroid[prev_ind][ind].y, 2));

		// exclude invalid larvae
		if (perimeter_temp < 7 // if perimeter is too short
			|| perimeter_temp > 11 // if perimeter is too long
			|| skeleton_length_temp < 2.7 // if spine is too short
			|| travel_distance > 2 // if centroid travelled too far
			|| getArea(contour[ind]) < 0.8 // if area of contour is too small
			|| isSelfIntersect(contour[ind]) // if contour is self-intersecting
			) {
			larvaeValid[ind] = false;
		}

		// find out if there was a valid frame for this larva in the past
		if (larvaeValid[ind]) {
			validInPresentOrPast[ind] = true;
		}
		if (validInPresentOrPast[ind] && existentInPast[ind]) {
			existentAndValidInPast[ind] = true;
		}
	}
}


// Correct head tail detection to reduce error rates
void correctHeadTailDetection(std::vector<std::vector<cv::Point2f> > & spine, std::vector<int> larvaeIDs, std::vector<bool> larvaeValid)
{
	// for all larvae
	for (int ind = 0; ind < larvaeIDs.size(); ++ind) {

		// get temporary head and tail from spine
		cv::Point2f head_temp = spine[ind][0];
		cv::Point2f tail_temp = spine[ind][nSpine - 1];

		if (larvaeValid[ind] && existentAndValidInPast[ind]) {

			// get distances between head and tail and master
			std::vector<double> dist_head_tail_prev(4);
			dist_head_tail_prev[0] = pow(master_head[ind].x - head_temp.x, 2) + pow(master_head[ind].y - head_temp.y, 2); // head to head
			dist_head_tail_prev[1] = pow(master_tail[ind].x - head_temp.x, 2) + pow(master_tail[ind].y - head_temp.y, 2); // head to tail
			dist_head_tail_prev[2] = pow(master_head[ind].x - tail_temp.x, 2) + pow(master_head[ind].y - tail_temp.y, 2); // tail to head
			dist_head_tail_prev[3] = pow(master_tail[ind].x - tail_temp.x, 2) + pow(master_tail[ind].y - tail_temp.y, 2); // tail to tail

																														  // find minimum of distances
			int min_ind = 0;
			for (int i = 0; i < dist_head_tail_prev.size(); ++i) {
				if (dist_head_tail_prev[i] < dist_head_tail_prev[min_ind]) {
					min_ind = i;
				}
			}

			bool flippedHeadTailOld = (votes_correct[ind] < votes_flipped[ind]);

			// update the coordinates of the proximity tracked points
			if (min_ind == 0 || min_ind == 3) // no flip has occurred
			{
				master_head[ind] = head_temp;
				master_tail[ind] = tail_temp;
				++votes_correct[ind];
			}
			else // flip has occurred
			{
				master_head[ind] = tail_temp;
				master_tail[ind] = head_temp;
				++votes_flipped[ind];
			}

			// flip spine as necessary
			if ((min_ind == 0 || min_ind == 3) != (votes_correct[ind] >= votes_flipped[ind])) {
				std::vector<cv::Point2f> spineTemp = spine[ind];
				for (int i = 0; i < nSpine; ++i) {
					spine[ind][i] = spineTemp[nSpine - 1 - i];
				}
			}

			// identifies frames in which head and tail were premanently flipped for this larva
			if ((votes_correct[ind] < votes_flipped[ind]) != flippedHeadTailOld) {
				flippedHeadTail[ind] = true;
			}
			else {
				flippedHeadTail[ind] = false;
			}
		}
		else if (larvaeValid[ind]) {
			// first valid frame
			master_head[ind] = head_temp;
			master_tail[ind] = tail_temp;
			++votes_correct[ind];
		}
	}
}


// Smooth spine over time
void smoothSpine(std::vector<std::vector<cv::Point2f> > spine, std::vector<int> larvaeIDs, std::vector<bool> larvaeValid)
{
	// initialize variables
	spine_smooth.resize(larvaeIDs.size());
	resizeFeaturePoint2fSpine(spine_prev);

	// for all larvae
	for (int ind = 0; ind < larvaeIDs.size(); ++ind) {
		spine_smooth[ind].resize(nSpine);
		for (int i = 0; i < nSpine; ++i) {
			// only smooth spine if data for this frame is of good quality
			if (larvaeValid[ind] && flippedHeadTail[ind] == false && existentAndValidInPast[ind] && weird[prev_ind][ind] == false) {
				spine_smooth[ind][i].x = gamma_spine * spine[ind][i].x + (1 - gamma_spine) * spine_prev[ind][i].x;
				spine_smooth[ind][i].y = gamma_spine * spine[ind][i].y + (1 - gamma_spine) * spine_prev[ind][i].y;
			}
			else if (larvaeValid[ind] == false) {
				spine_smooth[ind][i].x = spine_prev[ind][i].x;
				spine_smooth[ind][i].y = spine_prev[ind][i].y;
			}
			else {
				spine_smooth[ind][i].x = spine[ind][i].x;
				spine_smooth[ind][i].y = spine[ind][i].y;
			}
		}
	}
	spine_prev = spine_smooth;
}


// Update buffers
void updateBuffers(std::vector<int> larvaeIDs, double timeStamp)
{
	// update next_ind and prev_ind
	next_ind = (next_ind + 1) % nBuffers;
	if (next_ind == 0) {
		prev_ind = nBuffers - 1;
	}
	else {
		prev_ind = next_ind - 1;
	}

	// update time_stamp
	time_stamp[next_ind] = timeStamp - time_stamp_start;

	// match current and past larva IDs inside buffers
	larvaeIDsPastIndex.resize(larvaeIDs.size());
	existentInPast.resize(larvaeIDs.size());
	std::vector<int>::iterator pastIndexPtr;
	int pastIndex;
	for (int i = 0; i < larvaeIDs.size(); ++i) {
		pastIndexPtr = std::find(larvaeIDsPast.begin(), larvaeIDsPast.end(), larvaeIDs[i]);
		pastIndex = std::distance(larvaeIDsPast.begin(), pastIndexPtr);
		existentInPast[i] = !(pastIndex == larvaeIDsPast.size());
		larvaeIDsPastIndex[i] = pastIndex;
	}

	// resize features
	resizeAllFeatures();
}


// Clears buffers
void clearBuffers()
{
	// reset larvaeIDsPast
	larvaeIDsPast.resize(1);
	larvaeIDsPast[0] = 0;
	existentInPast.resize(1);
	existentInPast[0] = false;
	larvaeIDsPastIndex.resize(1);
	larvaeIDsPastIndex[0] = 0;

	// reset feature buffers
	resizeAllFeatures();
}

// Create larvae scorer
std::map<int, std::vector<int>> larvaeScorer;

// Define mean of vector function
float mean(std::vector<int> inputVector) {
	float count = 0;
	for (int i = 0; i<inputVector.size(); i++) {
		count += inputVector[i];
	}
	return (float)count / inputVector.size();
}

std::vector<int> getTopN(std::vector<float> meanScores, int n) {
	int highestIndex;
	const int N = sizeof(meanScores) / sizeof(int);
	std::vector<int> topNIndices(n);
	for (int i = 0; i<n; i++) {
		highestIndex = std::max_element(meanScores.begin(), meanScores.end()) - meanScores.begin();
		topNIndices[i] = highestIndex;
		//std::cout << "Index of max element: "<< highestIndex << std::endl;
		meanScores[highestIndex] = -1.0;
	}
	return topNIndices;
}

// Set all laser values to zero
void zeroAllLaserValues(std::vector<int> larvaeIDs) {
	for (int ind = 0; ind < larvaeIDs.size(); ++ind) {
		irIntensities[ind] = 0; // set all intensities to 0
		dmdIntensities[ind] = 0; // set all intensities to 0
	}
}

// Function to set laser values for top larvae only
void setLaser(float cur_intensity, std::string laserType, std::vector<int> larvaeIDs) {
	// Loop over top larvae and change intensity
	for (int ind = 0; ind < topLarvaeIndices.size(); ++ind) {
		if (laserType.compare("DMD") == 0) {
			dmdIntensities[topLarvaeIndices[ind]] = cur_intensity; // dmd_intensity; // set chosen larvae intensities to full
		}
		else if (laserType.compare("IR") == 0) {
			irIntensities[topLarvaeIndices[ind]] = cur_intensity;
		}
	}
}


void calculateStimulus(std::vector<int> larvaeIDs, std::string stimulationMode, std::string yokedFileName, int lightIntensity, double stim1, double stim2, double stim3, double stim4, double stim5, double stim6, double stim7, double stim8, double stim9, double stim10)
{
	// dmdIntensities and irIntensities are assigned values between 0 (light off) and 1 (light on)
	double dmd_intensity = lightIntensity / 100.0; // convert percentages to values between 0 and 1
	dmdIntensities.resize(larvaeIDs.size());
	irIntensities.resize(larvaeIDs.size());

	// -------------------------------------------------------------------------
	// Update larvae scores
	// -------------------------------------------------------------------------
	int score = 0;
	for (int ind = 0; ind < larvaeIDs.size(); ++ind) { // Loop over all larvae

		// Get scores
		score = 0;
		float behaviourValue = parallel_speed_tail_filtered[next_ind][ind];
		if (behaviourValue > .35) {
			score = 1;
		}

		// Ignore objects classed as "weird"
		float weirdValue = weird[next_ind][ind];
		if (weirdValue == 1) {
			score = 0;
		}
		else if (weirdValue == 0) { // but bias towards objects not classed as weird
			score += 1;
		}

		// Select against objects with a skeleton that is too large
		float sizeValue = skeleton_length[next_ind][ind];
		if (sizeValue > 10) {
			score = -1;
		}

		// Add score to list
		if (larvaeScorer.find(larvaeIDs[ind]) == larvaeScorer.end()) { // Add ID to list if it does not yet exist
			larvaeScorer.insert(std::pair<int, std::vector<int> >(larvaeIDs[ind], std::vector<int>{score}));
		}
		else {
			larvaeScorer[larvaeIDs[ind]].push_back(score);
		}
	}

	// -------------------------------------------------------------------------
	// Select which larvae to stimulate
	// -------------------------------------------------------------------------

	// Get mean scores
	std::vector<float> meanScores;
	for (int ind = 0; ind < larvaeIDs.size(); ++ind) {
		meanScores.push_back(mean(larvaeScorer[larvaeIDs[ind]]));
	}

	// Get indices of top mean scores
	topLarvaeIndices = getTopN(meanScores, larvaeToTrack);

	// -------------------------------------------------------------------------
	// Select stimulation mode
	// -------------------------------------------------------------------------

	if (yokedMode == false || yokedFileName.compare("none") == 0) {

		// -------------------------------------------------------
		// Delayed IR delivery (for time-dependent valence reversal learning)
		// -------------------------------------------------------
		if (stimulationMode.compare("DMD_IR_learning") == 0) {

			// Constants
			float waiting_time = stim1; // stim1
			int numberOfTrials = stim2;
			float ramp_pulse_duration = stim3;
			float topup_pulse_duration = stim4;
			float pulse_ISI = stim5;
			float time_between_stimulations = stim6;
			float lengthOfAllPulses = stim7; // seconds
			float onOff_period = topup_pulse_duration + pulse_ISI;
			int number_of_pulses = int(lengthOfAllPulses / onOff_period);
			float ir_intensity = 10000;
			float dmd_time_offset = stim8; // -5 seconds

			float dmd_stim_duration = 14;
			float testing_stim_duration = 10;
			float testing_stim_ISI = 10;
			float nOf_testing_stims = 6;
			float gap_from_test2train = 60;

			float training_trial_length = ramp_pulse_duration + (onOff_period * number_of_pulses) + time_between_stimulations;
			float testing_trial_length = testing_stim_duration + testing_stim_ISI;
			float testing_period = nOf_testing_stims * (testing_stim_duration + testing_stim_ISI);
			std::string laserType;

			// Define trial time
			float time = time_stamp[next_ind] - waiting_time;

			// Set all laser values zero as default
			zeroAllLaserValues(larvaeIDs);

			// ---------------------------------------------------------
			// TESTING PERIOD
			// ---------------------------------------------------------
			laserType = "DMD";
			float test_time = time_stamp[next_ind] - waiting_time;
			float pretest_plus_train_time = testing_period + (gap_from_test2train*2) + (training_trial_length*numberOfTrials);
			if (time > pretest_plus_train_time) {
				test_time -= pretest_plus_train_time;
			}

			int testing_trial_index = int(test_time / testing_trial_length);
			double testing_trial_time = test_time - (testing_trial_index*testing_trial_length);
			// Deliver time
			if (test_time < testing_period) {
				if ((testing_trial_time > 0) & (testing_trial_time < testing_stim_duration)) {
					setLaser(dmd_intensity, laserType, larvaeIDs);
				}
			}


			// ---------------------------------------------------------
			// TRAINING PERIOD
			// ---------------------------------------------------------
			float training_time = time_stamp[next_ind] - waiting_time - testing_period - gap_from_test2train;
			int training_trial_index = int(training_time / training_trial_length);
			double training_trial_time = training_time - (training_trial_index*training_trial_length);

			// If total number of trials have not been performed
			if (training_time <= (numberOfTrials * training_trial_length)) {

				////////////////////////////////
				// Deliver IR?
				////////////////////////////////
				laserType = "IR";

				// Ramp pulse
				if ((training_trial_time > 0) & (training_trial_time < ramp_pulse_duration)) {
					setLaser(ir_intensity, laserType, larvaeIDs);
				}
				else {
					setLaser(0, laserType, larvaeIDs);
				}

				// Top-up pulses
				if (training_trial_time > ramp_pulse_duration + pulse_ISI) {
					float pulseTime = training_trial_time - (ramp_pulse_duration + pulse_ISI);
					int pulseIndex = int(pulseTime / onOff_period);
					pulseTime -= (pulseIndex * onOff_period);
					if (pulseTime < topup_pulse_duration) {
						setLaser(ir_intensity, laserType, larvaeIDs);
					}
					if (pulseIndex >= number_of_pulses) {
						setLaser(0, laserType, larvaeIDs);
					}
				}

				////////////////////////////////
				// Deliver DMD?
				////////////////////////////////
				laserType = "DMD";

				if (training_trial_time > dmd_time_offset) {
					// Deliver any pre-stim
					if (training_trial_time > training_trial_length + dmd_time_offset) {
						setLaser(dmd_intensity, "DMD", larvaeIDs);
					} // During stim
					else if (training_trial_time - dmd_time_offset < dmd_stim_duration) {
						setLaser(dmd_intensity, "DMD", larvaeIDs);
					}
				}
			}
		}

		// -------------------------------------------------------
		// Just DMD (i.e. light) delivery
		// -------------------------------------------------------
		if (stimulationMode.compare("just_pulsed_DMD") == 0) {

			// Constants
			// waiting_time, nOf_testing_stims, testing_stim_duration, testing_stim_ISI
			float waiting_time = stim1; // stim1
			int nOf_testing_stims = stim2;
			float testing_stim_duration = stim3;
			float testing_stim_ISI = stim4; 
			float testing_trial_length = testing_stim_duration + testing_stim_ISI;
			float testing_period = nOf_testing_stims * (testing_stim_duration + testing_stim_ISI);
			std::string laserType;

			// Set all laser values zero as default
			zeroAllLaserValues(larvaeIDs);

			// ---------------------------------------------------------
			// TESTING PERIOD
			// ---------------------------------------------------------
			laserType = "DMD";
			float test_time = time_stamp[next_ind] - waiting_time;
			int testing_trial_index = int(test_time / testing_trial_length);
			double testing_trial_time = test_time - (testing_trial_index*testing_trial_length);
			// Deliver time
			if (test_time < testing_period) {
				if ((testing_trial_time > 0) & (testing_trial_time < testing_stim_duration)) {
					setLaser(dmd_intensity, laserType, larvaeIDs);
				}
			}
		}

		// -------------------------------------------------------
		// DMD declining in multiple steps
		// -------------------------------------------------------
		if (stimulationMode.compare("DMD_light_steps") == 0) {

			// Constants
			// waiting_time, nOf_testing_stims, testing_stim_duration, testing_stim_ISI
			float waiting_time = stim1; // stim1
			int nOf_testing_stims = stim2;
			float testing_stim_duration = stim3;
			float testing_stim_ISI = stim4;
			float dmd_step_length = stim5;
			float testing_trial_length = testing_stim_duration + testing_stim_ISI;
			float testing_period = nOf_testing_stims * (testing_stim_duration + testing_stim_ISI);
			std::string laserType;

			// Set all laser values zero as default
			zeroAllLaserValues(larvaeIDs);

			// ---------------------------------------------------------
			// TESTING PERIOD
			// ---------------------------------------------------------
			laserType = "DMD";
			float test_time = time_stamp[next_ind] - waiting_time;
			int testing_trial_index = int(test_time / testing_trial_length);
			double testing_trial_time = test_time - (testing_trial_index*testing_trial_length);
			// Deliver time
			if (test_time < testing_period) {
				if ((testing_trial_time > 0) & (testing_trial_time < testing_stim_duration)) {
					if ((testing_trial_time > 0) & (testing_trial_time < .25*testing_stim_duration)){
						setLaser(dmd_intensity, laserType, larvaeIDs);
					}
					else if ((testing_trial_time > .25*testing_stim_duration) & (testing_trial_time < .5*testing_stim_duration)) {
						setLaser(.75*dmd_intensity, laserType, larvaeIDs);
					}
					else if ((testing_trial_time > .5*testing_stim_duration) & (testing_trial_time < .75*testing_stim_duration)) {
						setLaser(.5*dmd_intensity, laserType, larvaeIDs);
					}
					else if ((testing_trial_time > .75*testing_stim_duration) & (testing_trial_time < testing_stim_duration)) {
						setLaser(.25*dmd_intensity, laserType, larvaeIDs);
					}
				}
			}
		}

		if (stimulationMode.compare("pulsed_ir") == 0) {

			// Constants
			float waiting_time = stim1; // stim1
			int numberOfTrials = stim2;
			float ramp_pulse_duration = stim3;
			float topup_pulse_duration = stim4;
			float pulse_ISI = stim5;
			float time_between_stimulations = stim6;
			float lengthOfAllPulses = stim7; // seconds
			int stimulationType = stim8; // 0=IRonly, 1=DMDonly
			float onOff_period = topup_pulse_duration + pulse_ISI;
			int number_of_pulses = int(lengthOfAllPulses / onOff_period);
			float trial_length = ramp_pulse_duration + (onOff_period * number_of_pulses) + time_between_stimulations;

			// Choose IR or DMD
			std::string laserType;
			int heatIntensity;
			if (stimulationType==0){
				laserType = "IR";
				heatIntensity = 10000;
			}
			else if (stimulationType == 1) {
				laserType = "DMD";
				heatIntensity = dmd_intensity;
			}

			// Time and trial variables
			float time = time_stamp[next_ind] - waiting_time;
			int trial_index = int(time / trial_length);
			double trial_time = time - (trial_index*trial_length);

			// Set laser to zero as default
			setLaser(0, laserType, larvaeIDs);

			// If total number of trials have not been performed
			if (time <= (numberOfTrials * trial_length)) {
				// Ramp pulse
				if ((trial_time > 0) & (trial_time < ramp_pulse_duration)) {
					setLaser(heatIntensity, laserType, larvaeIDs);
				}
				else {
					setLaser(0, laserType, larvaeIDs);
				}

				// Top-up pulses
				if (trial_time > ramp_pulse_duration + pulse_ISI) {
					float pulseTime = trial_time - (ramp_pulse_duration + pulse_ISI);
					int pulseIndex = int(pulseTime / onOff_period);
					pulseTime -= (pulseIndex * onOff_period);
					if (pulseTime < topup_pulse_duration) {
						setLaser(heatIntensity, laserType, larvaeIDs);
					}
					if (pulseIndex >= number_of_pulses) {
						setLaser(0, laserType, larvaeIDs);
					}
				}
			}
		}
	}

	//	if (stimulationMode.compare("open_loop_red") == 0) {

	//		double waiting_time = stim1;
	//		double pre_stimulus_interval = stim2;
	//		double stimulus_duration = stim3;
	//		double cycle_duration = stim4; // assumes cycle_duration > stimulus_duration
	//		int number_stimuli = int(stim5);

	//		bool lightOn = false;
	//		double effective_time = time_stamp[next_ind] - waiting_time;

	//		if (effective_time < pre_stimulus_interval) {
	//			lightOn = false;
	//		}
	//		else {
	//			double time = effective_time - pre_stimulus_interval;
	//			int index_stimuli = 1;
	//			while (time >= cycle_duration && index_stimuli < number_stimuli) {
	//				time -= cycle_duration;
	//				++index_stimuli;
	//			}
	//			if (time < stimulus_duration) {
	//				lightOn = true;
	//			}
	//			else {
	//				lightOn = false;
	//			}
	//		}

	//		//// Stop experiment?
	//		//double timeThreshold = 1.0;
	//		//if (effective_time > timeThreshold) {
	//		//	stopExperiment = 1;
	//		//}

	//		// Deliver stimulation
	//		if (lightOn) { // light on
	//			for (int ind = 0; ind < larvaeIDs.size(); ++ind) {
	//				dmdIntensities[ind] = 0; // set all intensities to 0
	//				irIntensities[ind] = 0; // set all intensities to 0
	//			}
	//			for (int ind = 0; ind < topLarvaeIndices.size(); ++ind) {
	//				dmdIntensities[topLarvaeIndices[ind]] = 0; // dmd_intensity; // set chosen larvae intensities to full
	//				irIntensities[topLarvaeIndices[ind]] = 10000; // set all intensities to 0 test
	//			}
	//		}
	//		else { // light off
	//			for (int ind = 0; ind < larvaeIDs.size(); ++ind) {
	//				dmdIntensities[ind] = 0;
	//				irIntensities[ind] = 0; // set all intensities to 0
	//			}
	//		}

	//		//// no IR stimulus
	//		//for (int ind = 0; ind < larvaeIDs.size(); ++ind) {
	//		//	irIntensities[ind] = 0;
	//		//}
	//	}
	//}
}


void generateOutputVector(std::vector<int> larvaeIDs, int * voteReset, int ImageHeight, double PixelResolution)
{
	int nVar = 4; // number of vectors sent
	outputVector.resize(2 + nVar * larvaeIDs.size());

	// load data into matrix
	std::vector<std::vector<int> > outputData(nVar);
	for (int i = 0; i < nVar; ++i) {
		outputData[i].resize(larvaeIDs.size());
	}

	// sort larvae by dmdIntensities
	// adapted from https://stackoverflow.com/questions/1577475/c-sorting-and-keeping-track-of-indexes
	std::vector<int> ind_ordered(larvaeIDs.size());
	int x = 0;
	std::iota(ind_ordered.begin(), ind_ordered.end(), x++); //Initializing
	std::sort(ind_ordered.begin(), ind_ordered.end(), [&](int i, int j) {return dmdIntensities[i]>dmdIntensities[j]; });

	for (int i = 0; i < ind_ordered.size(); ++i) {
		// i goes from 0 to n-1, ind denotes index of larva sorted by dmdIntensities in descending order
		int ind_loc = ind_ordered[i]; // get index of larva

		// // Single spot
		// outputData[0][i] = int(1000 * 100);
		// outputData[1][i] = int(1000 * (ImageHeight*PixelResolution / 1000.0 - 100));
		// outputData[2][i] = 5000; // spot radius [um]
		// outputData[3][i] = int(10000 * 1);
		// // Experimental mode (i.e. multi-spot)
		outputData[0][i] = int(1000 * centroid[next_ind][ind_loc].x); // x_neck_top [um]
		outputData[1][i] = int(1000 * (ImageHeight*PixelResolution / 1000.0 - centroid[next_ind][ind_loc].y)); // y_neck_top [um]
		outputData[2][i] = 5000; // spot radius [um]
		outputData[3][i] = int(10000 * dmdIntensities[ind_loc]); // relative light intensity on 0 to 10000 scale (uncalibrated)

		if (ball[next_ind][ind_loc] == true) { // ball classifier output (used to reset votes)
			voteReset[ind_loc] = 1;
		}
		else {
			voteReset[ind_loc] = 0;
		}
	}

	//for (int ind = 0; ind < larvaeIDs.size(); ++ind) {
	//	if (ball[next_ind][ind] == true) { // ball classifier output (used to reset votes)
	//		voteReset[ind] = 1;
	//	}
	//	else {
	//		voteReset[ind] = 0;
	//	}
	//	//stimulationFile << "ball " << ball[next_ind][ind] << " votereset " << voteReset[ind] << " ";
	//}
	////stimulationFile << std::endl;

	//for (int ind = 0; ind < larvaeIDs.size(); ++ind) {
	//	// i goes from 0 to n-1, ind denotes index of larva sorted by dmdIntensities in descending order
	//	outputData[0][ind] = int(1000 * neck_top[next_ind][ind].x); // x_neck_top [um]
	//	outputData[1][ind] = int(1000 * (ImageHeight*PixelResolution / 1000.0 - neck_top[next_ind][ind].y)); // y_neck_top [um]
	//	outputData[2][ind] = 5000; // spot radius [um]
	//	outputData[3][ind] = int(10000 * dmdIntensities[ind]); // relative light intensity on 0 to 10000 scale (uncalibrated)
	//	if (ball[next_ind][ind] == true) { // ball classifier output (used to reset votes)
	//		voteReset[ind] = 1;
	//	}
	//	else {
	//		voteReset[ind] = 0;
	//	}
	//}

	// load matrix into vector
	outputVector[0] = larvaeIDs.size();
	outputVector[1] = nVar;
	for (int ind = 0; ind < larvaeIDs.size(); ++ind) {
		for (int i = 0; i < nVar; ++i) {
			outputVector[i + ind * nVar + 2] = outputData[i][ind];
		}
	}

	//for (int ind = 0; ind < outputVector.size(); ++ind) {

	//	stimulationFile << outputVector[ind] << " ";
	//}

	//stimulationFile << std::endl;

}


std::string intVectorToString(std::vector<int> vec)
{
	std::ostringstream os;
	for (int i = 0; i < vec.size(); ++i) {
		os << vec[i];
		if (i < vec.size() - 1) {
			os << ",";
		}
	}

	std::string str(os.str());

	return str;
}


// Generate overall DLL output
void generateOutput(int* & outData, int* & galvoParameters, double PixelResolution, int ImageHeight)
{
	// initialize temporary galvo parameter vector
	std::vector<int> galvoParametersTemp(48);

	// set DMD output
	for (int i = 0; i < outputVector.size(); ++i) {
		outData[i] = outputVector[i];
	}

	// set galvo output (single galvo mode)
	int n_galvo_frames_per_cycle = n_larvae_per_galvo / 4;
	int frame_within_galvo_cycle = frame % n_galvo_frames_per_cycle;

	// grab new galvo parameters every n_galvo_frames_per_cycle frames
	if (frame_within_galvo_cycle == 0) {
		// set default temporary galvo output
		for (int i = 0; i < 16; ++i) {
			galvoParametersTemp[i * 3] = 1536; // x (pixels)
			galvoParametersTemp[i * 3 + 1] = 1536; // y (pixels)
			galvoParametersTemp[i * 3 + 2] = 0; // intensity (0-10000)
		}

		// overwrite default temporary galvo output for existing larvae
		for (int ind = 0; ind < irIntensities.size(); ++ind) {
			galvoParametersTemp[ind * 3] = int(1000 * centroid[next_ind][ind].x / PixelResolution); // x (pixels)
			galvoParametersTemp[ind * 3 + 1] = int(ImageHeight - 1000 * centroid[next_ind][ind].y / PixelResolution); // y (pixels)
			galvoParametersTemp[ind * 3 + 2] = 0; // irIntensities[ind]; // test
		}
	}

	//// generate galvo output for galvo 1 for this frame
	//for (int i = 0; i < 4; ++i) {
	//	galvoParameters[i * 3] = galvoParametersTemp[i * 3 + frame_within_galvo_cycle * 12]; // x (pixels)
	//	galvoParameters[i * 3 + 1] = galvoParametersTemp[i * 3 + frame_within_galvo_cycle * 12 + 1]; // y (pixels)
	//	galvoParameters[i * 3 + 2] = galvoParametersTemp[i * 3 + frame_within_galvo_cycle * 12 + 2]; // intensity (0-10000)
	//}

	// generate galvo output for galvo 1 for this frame (for just top larvae)
	for (int i = 0; i < 4; ++i) {
		galvoParameters[i * 3] = int(1000 * centroid[next_ind][topLarvaeIndices[i]].x / PixelResolution); // x (pixels)
		galvoParameters[i * 3 + 1] = int(ImageHeight - 1000 * centroid[next_ind][topLarvaeIndices[i]].y / PixelResolution); // y (pixels
		galvoParameters[i * 3 + 2] = irIntensities[topLarvaeIndices[i]]; // intensity (0-10000)
	}

	// generate (empty) galvo output for galvos 2-4 for this frame
	for (int i = 4; i < 16; ++i) {
		galvoParameters[i * 3] = 1536; // x (pixels)
		galvoParameters[i * 3 + 1] = 1536; // y (pixels)
		galvoParameters[i * 3 + 2] = 0; // intensity (0-10000)
	}

	//// Output galvo data
	//stimulationFile << dateTime << " "
	//	<< larvaeIDs[ind] << " "
	//	<< time_stamp[next_ind] << " "
	//	<< nContour[ind] << " "
	//	<< tictocdiff << " "
	//	<< dmdIntensities[ind] << " "
	//	<< irIntensities[ind] << std::endl;


	for (int i = 0; i < 16; ++i) {
		galvoFile << time_stamp[next_ind] << " "
			<< galvoParameters[i * 3] << " "
			<< galvoParameters[i * 3 + 1] << " "
			<< galvoParameters[i * 3 + 2] << std::endl;
	}

}


// Main function
void behaviorClassifier(char* dateTimePtr, int dateTimeLength, char* genotype1Ptr, int genotype1Length, char* genotype2Ptr, int genotype2Length,
	char* stimulationModePtr, int stimulationModeLength, char* rigNamePtr, int rigNameLength, int experimentStart, int experimentStop,
	int frameNum, double timeStamp, int numLarvae, int* larvaeIDsPtr, int* nContourPtr, int* xContourPtr, int* yContourPtr,
	int* xSpinePtr, int* ySpinePtr, int* larvaeValidPtr, int lightIntensity, char* savingPathPtr, int savingPathLength, char* yokedFileNamePtr, int yokedFileNameLength, int yokedFileEnable, double gamma_loc,
	double PixelResolution, int ImageHeight, int ImageWidth, int PixelIntensityLow, int PixelIntensityHigh, int BoxSideLow, int BoxSideHigh,
	int BoxWLLow, int BoxWLHigh, int AreaLow, int AreaHigh, int nSpineLoc, int BGSubTH, int minContourPoints,
	double stim1, double stim2, double stim3, double stim4, double stim5, double stim6, double stim7, double stim8, double stim9, double stim10,
	char* stimLabelsPtr, int stimLabelsLength, int* flipLarvaeIDPtr, int flipLarvaeIDLength, int laserIntensity, int* outData, int* voteReset, int* galvoParameters)
{
	// start timer
	tic();

	// Stop experiment?
	if (stopExperiment == 1) {
		experimentStop = 1;
		experimentStart = 0;
	}

	// initialize variables
	std::string dateTime = "";
	std::string genotype1 = "";
	std::string genotype2 = "";
	std::string rigName = "";
	std::string stimulationMode = "";
	savingPath = "";
	std::string yokedFileName = "";
	std::string stimLabels = "";
	std::vector<int> larvaeIDs;
	std::vector<int> nContour;
	std::vector<std::vector<cv::Point2f> > contour;
	std::vector<std::vector<cv::Point2f> > spine;
	std::vector<std::vector<cv::Point2f> > hull;
	std::vector<bool> larvaeValid;
	int numLarvaeCorrected;

	// read meta data
	readMetaData(dateTimePtr, dateTimeLength, genotype1Ptr, genotype1Length, genotype2Ptr, genotype2Length, rigNamePtr, rigNameLength,
		stimulationModePtr, stimulationModeLength, savingPathPtr, savingPathLength, yokedFileNamePtr, yokedFileNameLength,
		stimLabelsPtr, stimLabelsLength, dateTime, genotype1, genotype2, stimulationMode, rigName, savingPath, yokedFileName, stimLabels,
		yokedFileEnable, gamma_loc, nSpineLoc);

	// only executes in the first frame of an experiment
	if (experimentStart == 1 && experimentStop == 0) {

		time_stamp_start = timeStamp;

		frame = 0;

		// set random number generator seed
		std::srand(std::time(NULL));

		// close behavior file if still open
		if (behaviorFile.is_open() == true) {
			behaviorFile.close();
			behaviorFile.clear();
		}

		// close stimulation file if still open
		if (stimulationFile.is_open() == true) {
			stimulationFile.close();
			stimulationFile.clear();
		}

		// close hull file if still open
		if (hullFile.is_open() == true) {
			hullFile.close();
			hullFile.clear();
		}

		// close contour file if still open
		if (contourFile.is_open() == true) {
			contourFile.close();
			contourFile.clear();
		}

		// close spine file if still open
		if (spineFile.is_open() == true) {
			spineFile.close();
			spineFile.clear();
		}

		// close spine file if still open
		if (galvoFile.is_open() == true) {
			galvoFile.close();
			galvoFile.clear();
		}

		// open behavior file and overwrite existing file if any
		std::string behaviorPath(savingPath);
		behaviorPath = behaviorPath + "\\" + dateTime
			+ "@" + genotype1 + "@" + genotype2
			+ "@" + rigName + "@" + stimulationMode + "@" + std::to_string(lightIntensity) + ".behavior";
		behaviorFile.open(behaviorPath, std::ios::trunc);
		behaviorFile.close();
		behaviorFile.clear();
		behaviorFile.open(behaviorPath, std::ios::app);

		// open stimulation file and overwrite existing file if any
		std::string stimulationPath(savingPath);
		stimulationPath = stimulationPath + "\\" + dateTime
			+ "@" + genotype1 + "@" + genotype2
			+ "@" + rigName + "@" + stimulationMode + "@" + std::to_string(lightIntensity) + ".stimulation";
		stimulationFile.open(stimulationPath, std::ios::trunc);
		stimulationFile.close();
		stimulationFile.clear();
		stimulationFile.open(stimulationPath, std::ios::app);

		// open hull file and overwrite existing file if any
		std::string hullPath(savingPath);
		hullPath = hullPath + "\\" + dateTime
			+ "@" + genotype1 + "@" + genotype2
			+ "@" + rigName + "@" + stimulationMode + "@" + std::to_string(lightIntensity) + ".hull";
		hullFile.open(hullPath, std::ios::trunc);
		hullFile.close();
		hullFile.clear();
		hullFile.open(hullPath, std::ios::app);

		// open contour file and overwrite existing file if any
		std::string contourPath(savingPath);
		contourPath = contourPath + "\\" + dateTime
			+ "@" + genotype1 + "@" + genotype2
			+ "@" + rigName + "@" + stimulationMode + "@" + std::to_string(lightIntensity) + ".outline";
		contourFile.open(contourPath, std::ios::trunc);
		contourFile.close();
		contourFile.clear();
		contourFile.open(contourPath, std::ios::app);

		// open spine file and overwrite existing file if any
		std::string spinePath(savingPath);
		spinePath = spinePath + "\\" + dateTime
			+ "@" + genotype1 + "@" + genotype2
			+ "@" + rigName + "@" + stimulationMode + "@" + std::to_string(lightIntensity) + ".spine";
		spineFile.open(spinePath, std::ios::trunc);
		spineFile.close();
		spineFile.clear();
		spineFile.open(spinePath, std::ios::app);

		// open log file and overwrite existing file if any
		std::string galvoPath(savingPath);
		galvoPath = galvoPath + "\\" + dateTime
			+ "@" + genotype1 + "@" + genotype2
			+ "@" + rigName + "@" + stimulationMode + "@" + std::to_string(lightIntensity) + ".galvo";
		galvoFile.open(galvoPath, std::ios::trunc);
		galvoFile.close();
		galvoFile.clear();
		galvoFile.open(galvoPath, std::ios::app);

		// read stimulus file for yoked control if applicable
		if (yokedFileEnable > 0) {
			yokedMode = true;
			std::ifstream yokedFile(yokedFileName);
			std::string line;
			yokedFileContent.clear();
			while (std::getline(yokedFile, line)) {
				std::stringstream str_line(line);
				std::string string_element;
				std::vector<double> lineContent;
				while (std::getline(str_line, string_element, ',')) {
					lineContent.push_back(std::stod(string_element));
				}
				yokedFileContent.push_back(lineContent);
			}
			yokedFileLength = yokedFileContent.size();

			yokedLarvaIndices.clear();
			for (int i = 0; i < yokedFileLength; ++i) yokedLarvaIndices.push_back(i);
			std::shuffle(yokedLarvaIndices.begin(), yokedLarvaIndices.end(), std::default_random_engine(std::time(NULL)));
		}
		else {
			yokedMode = false;
			yokedFileContent.clear();
		}

		// write config file
		writeConfigFile(dateTime, genotype1, genotype2, stimulationMode, rigName, lightIntensity, laserIntensity, yokedFileName, stimLabels,
			PixelResolution, ImageHeight, ImageWidth, PixelIntensityLow, PixelIntensityHigh, BoxSideLow, BoxSideHigh,
			BoxWLLow, BoxWLHigh, AreaLow, AreaHigh, BGSubTH, minContourPoints, stim1, stim2, stim3, stim4, stim5, stim6, stim7, stim8, stim9, stim10);

		// clear buffers
		clearBuffers();
	}

	// initializes output vector
	outputVector.resize(1);
	outputVector[1] = 0;

	// only executes in the last frame of an experiment
	if (experimentStop == 1) {
		// close behavior file
		if (behaviorFile.is_open() == true) {
			behaviorFile.close();
			behaviorFile.clear();
		}

		// close stimulation file
		if (stimulationFile.is_open() == true) {
			stimulationFile.close();
			stimulationFile.clear();
		}

		// close hull file
		if (hullFile.is_open() == true) {
			hullFile.close();
			hullFile.clear();
		}

		// close contour file
		if (contourFile.is_open() == true) {
			contourFile.close();
			contourFile.clear();
		}

		// close spine file
		if (spineFile.is_open() == true) {
			spineFile.close();
			spineFile.clear();
		}
	}
	// if this is not the end of the experiment, execute behavior detection and generate stimulus
	else {
		readData(numLarvae, larvaeIDsPtr, nContourPtr, xContourPtr, yContourPtr, xSpinePtr, ySpinePtr,
			larvaeValidPtr, larvaeIDs, nContour, contour, spine, hull, larvaeValid, ImageHeight, PixelResolution, flipLarvaeIDPtr, flipLarvaeIDLength);
		updateBuffers(larvaeIDs, timeStamp);
		excludeInvalidLarvae(contour, spine, larvaeIDs, larvaeValid);
		correctHeadTailDetection(spine, larvaeIDs, larvaeValid);
		smoothSpine(spine, larvaeIDs, larvaeValid);
		behaviorDetection(contour, hull, larvaeIDs, larvaeValid);
		writeBehaviorFile(experimentStart, experimentStop, dateTime, larvaeIDs, larvaeValid);
		calculateStimulus(larvaeIDs, stimulationMode, yokedFileName, lightIntensity, stim1, stim2, stim3, stim4, stim5, stim6, stim7, stim8, stim9, stim10);
		writeStimulationFile(dateTime, larvaeIDs, nContour);
		//writeHullFile(dateTime, larvaeIDs, hull);
		writeContourFile(dateTime, larvaeIDs, contour);
		writeSpineFile(dateTime, larvaeIDs, spine);
		generateOutputVector(larvaeIDs, voteReset, ImageHeight, PixelResolution);

		// prepare for next frame
		larvaeIDsPast = larvaeIDs;
		++frame;
	}

	//outputVectorString = intVectorToString(outputVector);
	generateOutput(outData, galvoParameters, PixelResolution, ImageHeight);

	// REVIEW
	//std::vector<double> galvo1_x_calib(5);
	//std::vector<double> galvo1_y_calib(5);

	//galvo1_x_calib[0] = -2.98889377e-03;
	//galvo1_x_calib[1] = 5.63518652e-05;
	//galvo1_x_calib[2] = 1.46585334e-11;
	//galvo1_x_calib[3] = -5.38895263e-13;
	//galvo1_x_calib[4] = 0.3290812820206135;

	//galvo1_y_calib[0] = 1.08716887e-05;
	//galvo1_y_calib[1] = 2.91788940e-03;
	//galvo1_y_calib[2] = -3.05345965e-12;
	//galvo1_y_calib[3] = -1.53840182e-11;
	//galvo1_y_calib[4] = -2.1225909875996782;

	//galvoParameters[0] = 1600; // x (pixels)
	//galvoParameters[1] = 2000; // y (pixels)
	//galvoParameters[2] = 10000; // intensity (0-10000)

	//galvoParameters[3] = 2000; // x (pixels)
	//galvoParameters[4] = 1600; // y (pixels)
	//galvoParameters[5] = 10000; // intensity (0-10000)

	//galvoParameters[6] = 1600; // x (pixels)
	//galvoParameters[7] = 1600; // y (pixels)
	//galvoParameters[8] = 10000; // intensity (0-10000)

	//galvoParameters[9] = 2000; // x (pixels)
	//galvoParameters[10] = 2000; // y (pixels)
	//galvoParameters[11] = 10000; // intensity (0-10000)

	//double x_cam;
	//double y_cam;
	//int nVar = 4;

	//for (int ind = 0; ind < larvaeIDs.size(); ++ind) {
	//	//x_cam = outputVector[((ind + 8) % larvaeIDs.size()) * nVar + 2] / PixelResolution;
	//	//y_cam = outputVector[((ind + 8) % larvaeIDs.size()) * nVar + 3] / PixelResolution;
	//	
	//	// stimulationFile << x_cam << "\t" << y_cam << "\n";

	//	//galvoParameters[ind * 3] = (galvo1_x_calib[0] * x_cam
	//	//						+ galvo1_x_calib[1] * y_cam
	//	//						+ galvo1_x_calib[2] * x_cam*x_cam*x_cam
	//	//						+ galvo1_x_calib[3] * y_cam*y_cam*y_cam
	//	//						+ galvo1_x_calib[4]) * 1000; // x (mV)
	//	//galvoParameters[ind * 3 + 1] = (galvo1_y_calib[0] * x_cam
	//	//						+ galvo1_y_calib[1] * y_cam
	//	//						+ galvo1_y_calib[2] * x_cam*x_cam*x_cam
	//	//						+ galvo1_y_calib[3] * y_cam*y_cam*y_cam
	//	//						+ galvo1_y_calib[4]) * 1000; // y (mV)


	//	galvoParameters[ind * 3] = int(1000 * centroid[next_ind][ind].x / PixelResolution); // x (pixels)
	//	galvoParameters[ind * 3 + 1] = int(ImageHeight - 1000 * centroid[next_ind][ind].y / PixelResolution); // y (pixels)
	//	galvoParameters[ind * 3 + 2] = 12500; // t ON (us)

	//	//if (galvoParameters[ind * 3] > 10000) {
	//	//	galvoParameters[ind * 3] = 10000;
	//	//}
	//	//else if (galvoParameters[ind * 3] < -10000) {
	//	//	galvoParameters[ind * 3] = -10000;
	//	//}

	//	//if (galvoParameters[ind * 3 + 1] > 10000) {
	//	//	galvoParameters[ind * 3 + 1] = 10000;
	//	//}
	//	//else if (galvoParameters[ind * 3 + 1] < -10000) {
	//	//	galvoParameters[ind * 3 + 1] = -10000;
	//	//}

	//	//stimulationFile << galvoParameters[ind * 3] << "\t" << galvoParameters[ind * 3 + 1] << "\n";
	//}

	//// DELETE
	//if (time_stamp[next_ind] > 20) {
	//	for (int index = 0; index < 48; ++index) {
	//		galvoParameters[index] = galvoParametersOriginal[index];
	//	}
	//}
	//else {
	//	for (int index = 0; index < 48; ++index) {
	//		galvoParametersOriginal[index] = galvoParameters[index];
	//	}
	//}

	//galvoParameters[0] = 300;
	//galvoParameters[1] = 300;
	//galvoParameters[3] = 600;
	//galvoParameters[4] = 600;
	//galvoParameters[6] = 900;
	//galvoParameters[7] = 900;
	//galvoParameters[9] = 1200;
	//galvoParameters[10] = 1200;

	//for (int i = 0; i < 4; ++i) {			
	//	double x_cam = 1000 * centroid[next_ind][0].x / PixelResolution;
	//	double y_cam = 1000 * centroid[next_ind][0].y / PixelResolution;

	//	if (i % 2) {
	//		galvoParameters[i * 3] = galvo1_x_calib[0] * x_cam 
	//								+ galvo1_x_calib[1] * y_cam 
	//								+ galvo1_x_calib[2] * x_cam*x_cam
	//								+ galvo1_x_calib[3] * y_cam*y_cam
	//								+ galvo1_x_calib[4]; // x (mV)
	//		galvoParameters[i * 3 + 1] = galvo1_y_calib[0] * x_cam
	//								+ galvo1_y_calib[1] * y_cam
	//								+ galvo1_y_calib[2] * x_cam*x_cam
	//								+ galvo1_y_calib[3] * y_cam*y_cam
	//								+ galvo1_y_calib[4]; // y (mV)
	//		galvoParameters[i * 3 + 2] = 12500; // t ON (us)
	//	}
	//	else {
	//		galvoParameters[i * 3] = 0; // x (mV)
	//		galvoParameters[i * 3 + 1] = 0; // y (mV)
	//		galvoParameters[i * 3 + 2] = 12500; // t ON (us)
	//	}
	//	
	//}


	/*outData[0] = 16;
	outData[1] = 4;

	for (int i = 0; i < outData[0]; ++i) {
	outData[i*outData[1] + 2] = 6000 + 3500 * (16*(i % 4) + (int(10*time_stamp[next_ind]) % 16));
	outData[i*outData[1] + 3] = 6000 + 3500 * (16*int(i / 4) + int((int(10*time_stamp[next_ind]) / 16)));
	outData[i*outData[1] + 4] = 5000;
	outData[i*outData[1] + 5] = 10000;
	}
	*/

	hullFile << time_stamp[next_ind] << "\n";

	for (int i = 0; i < 48; ++i) {
		hullFile << galvoParameters[i] << "\t";
	}
	hullFile << "\n";


	toc();
}







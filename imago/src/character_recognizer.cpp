/****************************************************************************
 * Copyright (C) 2009-2012 GGA Software Services LLC
 * 
 * This file is part of Imago toolkit.
 * 
 * This file may be distributed and/or modified under the terms of the
 * GNU General Public License version 3 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 * 
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ***************************************************************************/

#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/foreach.hpp>
#include <boost/tuple/tuple.hpp>
#include <map>
#include <cmath>
#include <cfloat>
#include <deque>
#include <string.h> // memcpy

#include "stl_fwd.h"
#include "character_recognizer.h"
#include "segment.h"
#include "exception.h"
#include "scanner.h"
#include "segmentator.h"
#include "thin_filter2.h"
#include "image_utils.h"
#include "log_ext.h"
#include "recognition_tree.h"
#include "settings.h"
#include "fonts_list.h"
#include "file_helpers.h"
#include "platform_tools.h"

using namespace imago;

const std::string CharacterRecognizer::upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ$%^&#";
const std::string CharacterRecognizer::lower = "abcdefghijklmnopqrstuvwxyz";
const std::string CharacterRecognizer::digits = "0123456789";
const std::string CharacterRecognizer::brackets = "()[]";
const std::string CharacterRecognizer::charges = "+-";
const std::string CharacterRecognizer::all = CharacterRecognizer::upper + CharacterRecognizer::lower +
	                                         CharacterRecognizer::digits + CharacterRecognizer::charges + 
											 CharacterRecognizer::brackets + "=!";

const std::string CharacterRecognizer::like_bonds = "lL1iIVv";


bool imago::CharacterRecognizer::isPossibleCharacter(const Settings& vars, const Segment& seg, bool loose_cmp, char* result)
{
	RecognitionDistance rd = recognize(vars, seg, CharacterRecognizer::all);
	
	double best_dist;
	char ch = rd.getBest(&best_dist);

	if (result)
		*result = ch;

	if (ch == '!') // graphics
		return false;

	if (std::find(CharacterRecognizer::like_bonds.begin(), CharacterRecognizer::like_bonds.end(), ch) != CharacterRecognizer::like_bonds.end())
	{
		Points2i endpoints = SegmentTools::getEndpoints(seg);
		if ((int)endpoints.size() < vars.characters.MinEndpointsPossible)
		{
			return false;
		}
	}

	if (best_dist < vars.characters.PossibleCharacterDistanceStrong && 
		rd.getQuality() > vars.characters.PossibleCharacterMinimalQuality) 
	{
		return true;
	}

	if (loose_cmp && (best_dist < vars.characters.PossibleCharacterDistanceWeak 
		          && rd.getQuality() > vars.characters.PossibleCharacterMinimalQuality))
	{
		return true;
	}

	return false;
}

qword CharacterRecognizer::getSegmentHash(const Segment &seg, const std::string& candidates) 
{
	logEnterFunction();

	qword segHash = 0, shift = 0;
   
	// hash against the source candidates
	for (size_t i = 0; i < candidates.size(); i++)
		segHash ^= (candidates[i] << (i % (64-8)));
   
	// hash against the source pixels
	for (int y = 0; y < seg.getHeight(); y++)
	{
		for (int x = 0; x < seg.getWidth(); x++)
			if (seg.getByte(x,y) == 0) // ink
			{
				shift = (shift << 1) + x * 3 + y * 7;
				segHash ^= shift;
			}
	}

	return segHash;
}


void unpack(cv::Mat1d& result, const std::string& input, unsigned int factor)
{
	size_t dim = imago::CharacterRecognizerImp::REQUIRED_SIZE + 2 * imago::CharacterRecognizerImp::PENALTY_SHIFT;
	result = cv::Mat1d(dim, dim);
	for (size_t u = 0; u < dim; u++)
		for (size_t v = 0; v < dim; v++)
		{
			double value = (unsigned char)input[u * dim + v] - 32;
			if (factor != 1 && factor != 0)
				value /= factor;
			result[u][v] = value;
		}
}

void internalInitTemplates(imago::CharacterRecognizerImp::Templates& templates)
{
	unsigned int start = platform::TICKS();
	{
		#include "font.inc"
	}
	printf("Loaded font (%u entries) in %u ms\n", templates.size(), platform::TICKS() - start);
}


RecognitionDistance CharacterRecognizer::recognize(const Settings& vars, const Segment &seg, const std::string &candidates) const
{
	logEnterFunction();
		   
	getLogExt().appendSegment("Source segment", seg);
	getLogExt().append("Candidates", candidates);

	qword segHash = getSegmentHash(seg, candidates);	
	getLogExt().append("Segment hash", segHash);
	RecognitionDistance rec;
   
	if (vars.caches.PCacheSymbolsRecognition &&
		vars.caches.PCacheSymbolsRecognition->find(segHash) != vars.caches.PCacheSymbolsRecognition->end())
	{
		rec = (*vars.caches.PCacheSymbolsRecognition)[segHash];
		getLogExt().appendText("Used cache: clean");
	}
	else
	{
		static bool init = false;
		static CharacterRecognizerImp::Templates templates;
		
		if (!init)
		{
			internalInitTemplates(templates);
			init = true;
		}

		rec = CharacterRecognizerImp::recognizeImage(vars, seg, candidates, templates);
		getLogExt().appendMap("Font recognition result", rec);

		if (vars.caches.PCacheSymbolsRecognition)
		{
			(*vars.caches.PCacheSymbolsRecognition)[segHash] = rec;
			getLogExt().appendText("Filled cache: clean");
		}
	}

	if (getLogExt().loggingEnabled())
	{
		getLogExt().append("Result candidates", rec.getBest());
		getLogExt().append("Recognition quality", rec.getQuality());
	}

   return rec;
}


namespace imago
{
	namespace CharacterRecognizerImp
	{
		CircleOffsetPoints::CircleOffsetPoints(int radius)
		{
			clear();
			resize(radius+1);

			for (int dx = -radius; dx <= radius; dx++)
				for (int dy = -radius; dy <= radius; dy++)
				{
					int distance = imago::round(sqrt((double)(imago::square(dx) + imago::square(dy))));
					if (distance <= radius)
						at(distance).push_back(cv::Point(dx, dy));
				}
		}

		static CircleOffsetPoints offsets(REQUIRED_SIZE);

		void calculatePenalties(const cv::Mat1b& img, cv::Mat1d& penalty_ink, cv::Mat1d& penalty_white)
		{		
			int size = REQUIRED_SIZE + 2*PENALTY_SHIFT;
		
			penalty_ink = cv::Mat1d(size, size);
			penalty_white = cv::Mat1d(size, size);
		
			for (int y = -PENALTY_SHIFT; y < REQUIRED_SIZE + PENALTY_SHIFT; y++)
				for (int x = -PENALTY_SHIFT; x < REQUIRED_SIZE + PENALTY_SHIFT; x++)
				{
					for (int value = 0; value <= 255; value += 255)
					{
						double min_dist = REQUIRED_SIZE;
						for (size_t radius = 0; radius < offsets.size(); radius++)
							for (size_t point = 0; point < offsets[radius].size(); point++)
							{
								int j = y + offsets[radius].at(point).y;
								if (j < 0 || j >= img.cols)
									continue;
								int i = x + offsets[radius].at(point).x;
								if (i < 0 || i >= img.rows)
									continue;

								if (img(j,i) == value)
								{
									min_dist = radius;
									goto found;
								}
							}

						found:

						double result = (value == 0) ? min_dist : sqrt(min_dist);
					
						if (value == 0)
							penalty_ink(y + PENALTY_SHIFT, x + PENALTY_SHIFT) = result;
						else
							penalty_white(y + PENALTY_SHIFT, x + PENALTY_SHIFT) = result;
					}
				}	
		}

		double compareImages(const cv::Mat1b& img, const cv::Mat1d& penalty_ink, const cv::Mat1d& penalty_white)
		{
			double best = imago::DIST_INF;
			for (int shift_x = 0; shift_x <= 2 * PENALTY_SHIFT; shift_x += PENALTY_STEP)
				for (int shift_y = 0; shift_y <= 2 * PENALTY_SHIFT; shift_y += PENALTY_STEP)
				{
					double result = 0.0;
					double max_dist = REQUIRED_SIZE;
					for (int y = 0; y < img.cols; y++)
					{
						for (int x = 0; x < img.rows; x++)
						{
							int value = img(y,x);
							if (value == 0)
							{
								result += penalty_ink(y + PENALTY_SHIFT, x + PENALTY_SHIFT);
							}
							else
							{
								result += penalty_white(y + PENALTY_SHIFT, x + PENALTY_SHIFT);
							}
						}
					}
					if (result < best)
						best = result;
				}
			return best;
		}

		cv::Mat1b prepareImage(const Settings& vars, const cv::Mat1b& src, double &ratio)
		{
			cv::Mat1b temp_binary;
			cv::threshold(src, temp_binary, vars.characters.InternalBinarizationThreshold, 255, CV_THRESH_BINARY);

			imago::Image img_bin;
			imago::ImageUtils::copyMatToImage(temp_binary, img_bin);
			img_bin.crop();
			imago::ImageUtils::copyImageToMat(img_bin, temp_binary);		
		
			int size_y = temp_binary.rows;
			int size_x = temp_binary.cols;

			ratio = (double)size_x / (double)size_y;
		
			size_y = REQUIRED_SIZE;
			size_x = REQUIRED_SIZE;

			cv::resize(temp_binary, temp_binary, cv::Size(size_x, size_y), 0.0, 0.0, cv::INTER_CUBIC);

			cv::threshold(temp_binary, temp_binary, vars.characters.InternalBinarizationThreshold, 255, CV_THRESH_BINARY);
		
			return temp_binary;
		}	

		cv::Mat1b load(const std::string& filename)
		{
			const int CV_FORCE_GRAYSCALE = 0;
			return cv::imread(filename, CV_FORCE_GRAYSCALE);
		}

		std::string upper(const std::string& in)
		{
			std::string data = in;
			std::transform(data.begin(), data.end(), data.begin(), ::toupper);
			return data;
		}

		std::string lower(const std::string& in)
		{
			std::string data = in;
			std::transform(data.begin(), data.end(), data.begin(), ::tolower);
			return data;
		}

		std::string convertFileNameToLetter(const std::string& name)
		{
			std::string temp = name;
			strings levels;

			for (size_t u = 0; u < 3; u++)
			{
				size_t pos_slash = file_helpers::getLastSlashPos(temp);

				if (pos_slash == std::string::npos)
				{
					break;
				}

				std::string sub = temp.substr(pos_slash+1);
				levels.push_back(sub);
				temp = temp.substr(0, pos_slash);			
			}

		
			if (levels.size() >= 3)
			{
				if (lower(levels[2]) == "capital")
					return upper(levels[1]);
				else if (lower(levels[2]) == "special")
					return levels[1];
				else
					return lower(levels[1]);
			}
			else if (levels.size() >= 2)
			{
				return lower(levels[1]);
			}

			// failsafe
			return name;
		}

		bool initializeTemplates(const Settings& vars, const std::string& path, Templates& templates)
		{
			strings files;
			file_helpers::getDirectoryContent(path, files, true);
			file_helpers::filterOnlyImages(files);
			for (size_t u = 0; u < files.size(); u++)
			{
				MatchRecord mr;
				cv::Mat1b image = load(files[u]);
				mr.text = convertFileNameToLetter(files[u]);
				if (!image.empty())
				{
					cv::Mat1b prepared = prepareImage(vars, image, mr.wh_ratio);
					calculatePenalties(prepared, mr.penalty_ink, mr.penalty_white);
					templates.push_back(mr);
				}
			}
			return !templates.empty();
		}

		struct ResultEntry
		{
			double value;
			std::string text;
			ResultEntry(double _value, std::string _text)
			{
				value = _value;
				text = _text;
			}
			bool operator <(const ResultEntry& second)
			{
				if (this->value < second.value)
					return true;
				else
					return false;
			}
		};

		imago::RecognitionDistance recognizeMat(const Settings& vars, const cv::Mat1b& rect, const std::string &candidates, const Templates& templates)
		{
			imago::RecognitionDistance _result;

			std::vector<ResultEntry> results;
		
			double ratio;
			cv::Mat1b img = prepareImage(vars, rect, ratio);

			for (size_t u = 0; u < templates.size(); u++)
			{
				if (!candidates.empty() && candidates.find(templates[u].text) == std::string::npos)
					continue;

				try
				{
					double distance = compareImages(img, templates[u].penalty_ink, templates[u].penalty_white);
					double ratio_diff = imago::absolute(ratio - templates[u].wh_ratio);
				
					if (ratio_diff > 0.3) // TODO: ratio constants
						distance *= 1.1;
					else if (ratio_diff > 0.5)
						distance *= 1.3;

					results.push_back(ResultEntry(distance, templates[u].text));
				}
				catch(std::exception& e)
				{
					printf("Exception: %s\n", e.what());
				}
			}

			if (results.empty())
				return _result;

			std::sort(results.begin(), results.end());

			for (int u = std::min(vars.characters.MaxTopVariantsLookup, (int)results.size() - 1); u >= 0; u--)
			{
				if (results[u].text.size() == 1)
				{
					_result[results[u].text[0]] = results[u].value / vars.characters.DistanceScaleFactor;
				}
			}

			return _result;
		}

		RecognitionDistance recognizeImage(const Settings& vars, const imago::Image& img, const std::string &candidates, const Templates& templates)
		{		
			cv::Mat1b rect;
			imago::ImageUtils::copyImageToMat(img, rect);
			return recognizeMat(vars, rect, candidates, templates);
		}
	}
}

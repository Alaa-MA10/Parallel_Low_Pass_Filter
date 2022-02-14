#include <mpi.h>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include<string.h>
#include<msclr\marshal_cppstd.h>
#include <ctime>// include this header 
#pragma once

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
using namespace std;
using namespace msclr::interop;

int* inputImage(int* w, int* h, System::String^ imagePath) //put the size of image in w & h
{
	int* input;


	int OriginalImageWidth, OriginalImageHeight;

	//*********************************************************Read Image and save it to local arrayss*************************	
	//Read Image and save it to local arrayss

	System::Drawing::Bitmap BM(imagePath);

	OriginalImageWidth = BM.Width;
	OriginalImageHeight = BM.Height;
	*w = BM.Width;
	*h = BM.Height;
	int *Red = new int[BM.Height * BM.Width];
	int *Green = new int[BM.Height * BM.Width];
	int *Blue = new int[BM.Height * BM.Width];
	input = new int[BM.Height*BM.Width];
	for (int i = 0; i < BM.Height; i++)
	{
		for (int j = 0; j < BM.Width; j++)
		{
			System::Drawing::Color c = BM.GetPixel(j, i);

			Red[i * BM.Width + j] = c.R;
			Blue[i * BM.Width + j] = c.B;
			Green[i * BM.Width + j] = c.G;

			input[i*BM.Width + j] = ((c.R + c.B + c.G) / 3); //gray scale value equals the average of RGB values

		}

	}
	return input;
}


void createImage(int* image, int width, int height, int index)
{
	System::Drawing::Bitmap MyNewImage(width, height);


	for (int i = 0; i < MyNewImage.Height; i++)
	{
		for (int j = 0; j < MyNewImage.Width; j++)
		{
			//i * OriginalImageWidth + j
			if (image[i*width + j] < 0)
			{
				image[i*width + j] = 0;
			}
			if (image[i*width + j] > 255)
			{
				image[i*width + j] = 255;
			}
			System::Drawing::Color c = System::Drawing::Color::FromArgb(image[i*MyNewImage.Width + j], image[i*MyNewImage.Width + j], image[i*MyNewImage.Width + j]);
			MyNewImage.SetPixel(j, i, c);
		}
	}
	MyNewImage.Save("..//Data//Output//outputRes3" + index + ".png");
	cout << "result Image Saved " << index << endl;
}



int* pad_image(int* image, int width, int height, int filter_size, int pad_size, int padded_width)
{
	int*padded_image = new int[pad_size]();

	int half_filter_size = (filter_size - 1) / 2;

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			padded_image[(i + half_filter_size) * (padded_width)+(j + half_filter_size)] = image[i * width + j];
		}
	}

	return padded_image;
}


int* low_pass_filter(int* image, int sub_size, int height, int start_idx, bool Last_processor, int filter_size) //average filtering
{
	int* result = new int[sub_size]();
	int mask_size = pow(filter_size, 2);;
	int* mask = new int(mask_size);

	int width = sub_size / height;
	int half_filter_size = (filter_size - 1) / 2;
	int start_height = 0, end_height = height;
	int start_width = half_filter_size, end_width = width - half_filter_size;

	if (start_idx == 0)
		start_height = half_filter_size;

	else if (Last_processor)
		end_height = height - half_filter_size;

	//Move mask through all elements of the image
	for (int h = start_height; h < end_height; h++)
	{
		for (int w = start_width; w < end_width; w++)
		{
			int start_mask_height = h - half_filter_size, end_mask_height = h + half_filter_size;
			int start_mask_width = w - half_filter_size, end_mask_width = w + half_filter_size;

			// select mask elements
			int k = 0;
			for (int j = start_mask_height; j <= end_mask_height; j++)
			for (int i = start_mask_width; i <= end_mask_width; i++)
				mask[k++] = image[(j * width + i) + start_idx];

			int sum = 0;
			for (int i = 0; i < mask_size; i++)
				sum += mask[i];

			// Calc the average
			result[(h * width) + w] = sum / mask_size;
		}
	}
	return result;
}


int main()
{
	int ImageWidth = 4, ImageHeight = 4;
	int start_s, stop_s, TotalTime = 0;
	int ProcessorId, Processors_num, master_id = 0;
	int padded_width, padded_height, padded_image_size;
	int rows_per_processor, start_idx;
	int sub_image_size;
	int filter_size;

	bool Last_processor = FALSE;

	int* imageData = NULL;
	int* padded_image = NULL;
	int* sub_filtered_image = NULL;
	int* result_image = NULL;

	const int Tag_Start_idx = 1;
	const int Tag_Processor_Rows = 2;
	const int Tag_Last_Processor = 3;
	const int Tag_Filter_Size = 4;
	const int Tag_Sub_Filtered = 5;
	const int Tag_Sub_Size = 6;

	MPI_Status RecvStatus;

	System::String^ imagePath;
	std::string img;
	img = "..//Data//Input//test3.jpg";

	start_s = clock();
	//Initialize the MPI environment..
	MPI_Init(NULL, NULL);
	MPI_Comm_rank(MPI_COMM_WORLD, &ProcessorId);

	if (ProcessorId == master_id)
	{
		cout << "Enter number of processors (Master not working): ";
		cin >> Processors_num;

		cout << "Enter size of Filter (odd number): ";
		cin >> filter_size;

		// reading image
		cout << "... Reading image ...\n";
		imagePath = marshal_as<System::String^>(img);
		imageData = inputImage(&ImageWidth, &ImageHeight, imagePath);

		// padding image
		cout << "... Padding image...\n";
		padded_width = ImageWidth + filter_size - 1;
		padded_height = ImageHeight + filter_size - 1;
		padded_image_size = padded_width * padded_height;

		padded_image = new int[padded_image_size]();
		padded_image = pad_image(imageData, ImageWidth, ImageHeight, filter_size, padded_image_size, padded_width);

		//find the number of rows per process except master
		rows_per_processor = padded_height / Processors_num - 1;

		//send the data to the rest of the processes ..
		for (int i = 1; i < Processors_num; i++)
		{
			//(1) Calc start index foreach Processor
			start_idx = (i - 1) * (padded_width * rows_per_processor);

			if (i == Processors_num - 1)
			{
				// last process takes the rest of Rows, and flag = True
				rows_per_processor = padded_height - (rows_per_processor * (Processors_num - 2));
				Last_processor = TRUE;
			}

			//(2) Calc each Processor (image size)
			sub_image_size = padded_width * rows_per_processor;

			//(3) send data
			MPI_Send(&rows_per_processor, 1, MPI_INT, i, Tag_Processor_Rows, MPI_COMM_WORLD);
			MPI_Send(&sub_image_size, 1, MPI_INT, i, Tag_Sub_Size, MPI_COMM_WORLD);
			MPI_Send(&start_idx, 1, MPI_INT, i, Tag_Start_idx, MPI_COMM_WORLD);
			MPI_Send(&Last_processor, 1, MPI_INT, i, Tag_Last_Processor, MPI_COMM_WORLD);
			MPI_Send(&filter_size, 1, MPI_INT, i, Tag_Filter_Size, MPI_COMM_WORLD);
		}

		cout << "Master done!!\n";
	}	// END master work

	else
	{
		// Recieve data foreach Processor
		MPI_Recv(&rows_per_processor, 1, MPI_INT, master_id, Tag_Processor_Rows, MPI_COMM_WORLD, &RecvStatus);
		MPI_Recv(&sub_image_size, 1, MPI_INT, master_id, Tag_Sub_Size, MPI_COMM_WORLD, &RecvStatus);
		MPI_Recv(&start_idx, 1, MPI_INT, master_id, Tag_Start_idx, MPI_COMM_WORLD, &RecvStatus);
		MPI_Recv(&Last_processor, 1, MPI_INT, master_id, Tag_Last_Processor, MPI_COMM_WORLD, &RecvStatus);
		MPI_Recv(&filter_size, 1, MPI_INT, master_id, Tag_Filter_Size, MPI_COMM_WORLD, &RecvStatus);
	}

	MPI_Bcast(&padded_image_size, 1, MPI_INT, master_id, MPI_COMM_WORLD);

	if (ProcessorId != master_id)
		padded_image = new int[padded_image_size]();

	MPI_Bcast(padded_image, padded_image_size, MPI_INT, master_id, MPI_COMM_WORLD);

	if (ProcessorId != master_id)
	{
		sub_filtered_image = new int[sub_image_size]();
		// Apply low pass filter
		sub_filtered_image = low_pass_filter(padded_image, sub_image_size, rows_per_processor, start_idx, Last_processor, filter_size);
		cout << "Processor " << ProcessorId << " says.. filtering DONE!" << endl;

		// send (sub filtered, size, start index) to master
		MPI_Send(&sub_image_size, 1, MPI_INT, master_id, Tag_Sub_Size, MPI_COMM_WORLD);
		MPI_Send(sub_filtered_image, sub_image_size, MPI_INT, master_id, Tag_Sub_Filtered, MPI_COMM_WORLD);
		MPI_Send(&start_idx, 1, MPI_INT, master_id, Tag_Start_idx, MPI_COMM_WORLD);
		cout << "Processor " << ProcessorId << " says..send filtered Img!" << endl;
	}
	else
	{
		result_image = new int[padded_image_size]();

		int finished_processor = 0;
		while (finished_processor < Processors_num - 1)
		{
			// Recieve (sub filtered, size , start index) from each processor
			MPI_Recv(&sub_image_size, 1, MPI_INT, MPI_ANY_SOURCE, Tag_Sub_Size, MPI_COMM_WORLD, &RecvStatus);
			int Source = RecvStatus.MPI_SOURCE;

			sub_filtered_image = new int[sub_image_size]();
			MPI_Recv(sub_filtered_image, sub_image_size, MPI_INT, Source, Tag_Sub_Filtered, MPI_COMM_WORLD, &RecvStatus);
			MPI_Recv(&start_idx, 1, MPI_INT, Source, Tag_Start_idx, MPI_COMM_WORLD, &RecvStatus);
			cout << "recieved sub Img data in master done!!\n";

			// combine each (sub_filtered) to result image
			int s = 0;
			for (int i = start_idx; i < start_idx + sub_image_size; ++i)
			{
				result_image[i] = sub_filtered_image[s++];

			}
			finished_processor++;
		}
		createImage(result_image, padded_width, padded_height, 0);
		cout << "create done!!\n";

		stop_s = clock();
		TotalTime += (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;
		cout << "time: " << TotalTime << endl;
	}

	free(imageData);
	free(padded_image);
	free(sub_filtered_image);
	free(result_image);
	// Finalize the MPI environment
	MPI_Finalize();
	return 0;
}
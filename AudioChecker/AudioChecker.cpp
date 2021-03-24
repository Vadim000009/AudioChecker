#define _CRT_SECURE_NO_WARNINGS	// не видеть ругательств компилятора
#include <windows.h>
#define ENABLE_SNDFILE_WINDOWS_PROTOTYPES 1
#include "sndfile.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <string>
#define BLOCK_SIZE 2

using doubleByte = unsigned char;
using namespace std;

typedef unsigned char(__cdecl *Point) (short a, short b, short c, short *d);
void getAmplitudesArray(string *fstFileToCompare, string *secFileToCompare, int *channels);
void compareMono(vector<short> *fstArr, vector<short> *secArr);
void compareStereo(vector<short> *leftFstArr, vector<short> *rightFstArr, vector<short> *leftSecArr, vector<short> *rightSecArr);
void compareWAVfiles(string *fstFileToCompare, string *secFileToCompare);
short readAmplitudesFromWAV(string fileName);


/* Функция, отвечающая за сравнение двух wav файлов.
На вход поступает две строки, которые являются названиями файлов. После их получения происходит 
их считывание в массив и последующая процедура вызова сравнения.
@fstFileToCompare и @secFileToCompare являются строками с названиями файла.
По итогу работы, в месте запуска программы создаётся файл compareWAV.dat с резульататми сравнения двух файлов. */
void compareWAVfiles(string *fstFileToCompare, string *secFileToCompare) {
	short fstArr = readAmplitudesFromWAV(*fstFileToCompare);
	short *secArr;
}

short readAmplitudesFromWAV(string fileName) {
	SF_INFO fileInfo;
	SNDFILE *fileWAV = NULL;
	if ((fileWAV = sf_open(fileName.c_str(), SFM_READ, &fileInfo)) = NULL) {
		cout << "Файл не найден! Работа прекращена";
		return 1;
	}
	short *array = new short[BLOCK_SIZE*fileInfo.frames];
	int count = 0;
	static_cast<short>(sf_read_short(fileWAV, array, BLOCK_SIZE));
	for (int i = 0; i < BLOCK_SIZE*fileInfo.frames; i++)
	{
		cout << array[i] << endl;
	}
	sf_close(fileWAV);
	return *array;
}

static void readFile(SNDFILE *fileWAV, SF_INFO fileInfo) {
	int frames = fileInfo.frames;
	short *leftChennel = new short[BLOCK_SIZE*frames];
	short *rightChannel = new short[BLOCK_SIZE*frames];
	short *buf = new short[BLOCK_SIZE * BLOCK_SIZE];
	int k = 0, m, readcount;
	FILE *fout = fopen("listKal.dat", "w");
	while (readcount = static_cast<short>(sf_read_short(fileWAV, buf, BLOCK_SIZE)) > 0) {
			//leftChennel[k] = buf[k*channels + m];
			k++;
			rightChannel[k] = buf[1];
			k++;
	}
	delete[] leftChennel;
	delete[] rightChannel;
	fclose(fout);
	sf_close;
	return;
}

/* Функция, получающая амплитуды из двух файлов и загоняющая их в векторный массив.
@fstFileToCompare и @secFileToCompare являются указателями на файлы,из которых будут полученны амплитуды.
@channels является указателем на количество каналов аудио. 
Возможны случаи получения для моно и стерео. */
void getAmplitudesArray(string *fstFileToCompare, string *secFileToCompare, int *channels) {
	ifstream fstOpen, secOpen;
	short amplitude = 0;
	try {
		fstOpen.open(fstFileToCompare->c_str(), ios_base::in);
		secOpen.open(secFileToCompare->c_str(), ios_base::in);
	}
	catch (...) {
		cout << "Ошибка открытия файла! Программа будет закрыта\n";
		fstOpen.close();
		secOpen.close();
		return;
	}
	if (*channels == 1) {
		vector <short> monoChannelFstFile;
		vector <short> monoChannelSecFile;
		// Необходимо сделать параллельным считывание из обоих файлов
		while (!fstOpen.eof()) {
			fstOpen >> amplitude;
			monoChannelFstFile.push_back(amplitude);
		}
		while (!secOpen.eof()) {
			secOpen >> amplitude;
			monoChannelSecFile.push_back(amplitude);
		}
		cout << "Чтение прошло успешно!\n" << "Количество амплитуд первого файла: " << monoChannelFstFile.size() <<
			". Количество амплитуд второго файла: " << monoChannelSecFile.size();
		compareMono(&monoChannelFstFile, &monoChannelSecFile);
	} else if (*channels == 2) {
		vector <short> leftChannelFstFile;
		vector <short> rightChannelFstFile;
		vector <short> leftChannelSecFile;
		vector <short> rightChannelSecFile;
		bool channelFst = true, channelSec = true;
		// Необходимо сделать параллельным считывание из обоих файлов
		while (!fstOpen.eof()) {
			fstOpen >> amplitude;
			if (channelFst) {
				leftChannelFstFile.push_back(amplitude);
				channelFst = false;
			} else {
				rightChannelFstFile.push_back(amplitude);
				channelFst = true;
			}
		}
		while (!secOpen.eof()) {
			secOpen >> amplitude;
			if (channelSec) {
				leftChannelSecFile.push_back(amplitude);
				channelSec = false;
			} else {
				rightChannelSecFile.push_back(amplitude);
				channelSec = true;
			}
		}
		cout << "Чтение прошло успешно!\n" << "Количество амплитуд первого файла: " << leftChannelFstFile.size() + rightChannelFstFile.size() <<
			". Количество амплитуд второго файла: " << leftChannelSecFile.size() + rightChannelSecFile.size();
		compareStereo(&leftChannelFstFile, &rightChannelFstFile, &leftChannelSecFile, &rightChannelSecFile);
	}
	fstOpen.close();
	secOpen.close();
}

/* Функция сравнения амплитуд в моно режиме. Создаёт файл resultMono.dat с результатами сравнения и указанием различия в определённых точках,
а так же в процентном соотношении об их разнице.
@fstArr и @secArr являются указателями на массивы векторов со значениями амплитуд. */
void compareMono(vector<short> *fstArr, vector<short> *secArr) {
	ofstream fileResult("resultMono.dat", ios::app | ios::out);
	int count = 0, size = fstArr->size();
	if (fileResult.is_open()) {
		for (int i = 0; i != size; i++) {
			if (fstArr->at(i) != secArr->at(i)) {
				fileResult << "Позиция\t" << i << " значения\t" << fstArr->at(i) <<"\tи\t" << secArr->at(i) << endl;
				count++;
			}
		}
		double resultCompare = (100 / (double)size) * (double)count;
		fileResult << "По результатам сравнения, различия файлов составили " << setprecision(3) << resultCompare << "%" << endl;
	} else {
		cout << "Ошибка создания файла \"result.dat\"! Программа будет завершена.";
	}
	fileResult.close();
} 

/* Функция сравнения амплитуд в стерео режиме. Создаёт файл resultMono.dat с результатами сравнения и указанием различия в определённых точках,
а так же в процентном соотношении об их разнице.
@leftFstArr и @rightFstArr являются указателями на массивы векторов первого массива со значениями амплитуд. 
@leftSecArr и @rightSecArr являются указателями на массивы векторов второго массива со значениями амплитуд. */
void compareStereo(vector<short> *leftFstArr, vector<short> *rightFstArr, vector<short> *leftSecArr, vector<short> *rightSecArr) {
	ofstream fileResult("resultStereo.dat", ios::app | ios::out);
	int countLeft = 0, sizeLeft = leftFstArr->size(), countRight = 0, sizeRight = rightSecArr->size();
	if (fileResult.is_open()) {
		for (int i = 0; i != sizeLeft; i++) {
			if (leftFstArr->at(i) != leftSecArr->at(i)) {
				fileResult << "Позиция левого канала\t" << i << " значения\t" << leftFstArr->at(i) << "\tи\t" << leftSecArr->at(i) << endl;
				countLeft++;
			}
		}
		double resultCompareL = (100 / (double)sizeLeft) * (double)countLeft;
		fileResult << "По результатам сравнения, различия файлов в левом канале составили " << setprecision(3) << resultCompareL << "%" << endl;
		for (int i = 0; i != sizeRight; i++) {
			if (rightFstArr->at(i) != rightSecArr->at(i)) {
				fileResult << "Позиция правого канала\t" << i << " значения\t" << rightFstArr->at(i) << "\tи\t" << rightSecArr->at(i) << endl;
				countRight++;
			}
		}
		double resultCompareR = (100 / (double)sizeRight) * (double)countRight;
		fileResult << "По результатам сравнения, различия файлов в правом канале составили " << setprecision(3) << resultCompareR << "%" << endl;
	} else {
		cout << "Ошибка создания файла \"result.dat\"! Программа будет завершена.";
	}
	fileResult.close();
}


int main(int argc, char* argv[]) {
	setlocale(LC_ALL, "rus");
	char compare[8] = "compare", compareWAV[11] = "compareWAV";
	if (argc > 1 && strcmp(argv[1], compare)) {
		string fstFileToCompare = argv[2];
		string secFileToCompare = argv[3];
		//string channelsString = argv[3];
		int channels = atoi(argv[4]);
		getAmplitudesArray(&fstFileToCompare, &secFileToCompare, &channels);
	} else if (argc > 1 && strcmp(argv[1], compareWAV)) {
		string fstFileToCompare = argv[2];
		string secFileToCompare = argv[3];
		compareWAVfiles(&fstFileToCompare, &secFileToCompare);
	}
	string fstFile = "", secFile = "";
	int channels = 0;
	cout << "Введите название первого файла для сравнения: ";
	getline(cin, fstFile);
	cout << "Введите название второго файла для сравнения: ";
	getline(cin, secFile);
	cout << "Укажите количество каналов аудио обоих файлов: ";
	cin >> channels;
	getAmplitudesArray(&fstFile, &secFile, &channels);
	//-------------------------------
	// Coldplay - Viva La Vida (low).wav
	//string file = "Marlena Shaw - California Soul.wav";
	//string file = "listLow.dat";
	//string file2 = "listLow2.dat";
	//int channels = 2;
	//cout << "Введите название файла\n";
	//getAmplitudesArray(&file, &file2, &channels);
//	getline(cin, file);
//	SNDFILE *fileWAV = NULL;
//	SF_INFO sfinfo;

//	wstring wide_string = wstring(file.begin(), file.end());
//	const wchar_t* out = wide_string.c_str();

//	char *chrstr = new char[file.length() + 1];	// из String в char
//	strcpy(chrstr, file.c_str());
	
//	if ((fileWAV = sf_wchar_open(out, SFM_READ, &sfinfo)) == NULL)
//	{
//		printf("Not able to open file %s.\n", out);
//		puts(sf_strerror(NULL));
//		return 1;
//	}
//	cout << sfinfo.format << endl;
//	cout << sfinfo.channels << endl;
//	cout << sfinfo.samplerate << endl;
//	cout << sfinfo.frames << endl;
//	readFile(fileWAV, sfinfo);
	// На вход поступает три аргумента ПОКА ЧТО. Название двух файлов с амплитудами и количество каналов.

	return 0;

}

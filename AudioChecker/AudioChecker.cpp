#define _CRT_SECURE_NO_WARNINGS	// не видеть ругательств компилятора
#include <windows.h>
#define ENABLE_SNDFILE_WINDOWS_PROTOTYPES 1
#include "sndfile.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <string>
#include <thread>
#define BLOCK_SIZE 2

using doubleByte = unsigned char;
using namespace std;

// Прототипы функций. Функция Point - для вычисления промежуточных точек, пока не подключена.
typedef unsigned char(__cdecl *Point) (short a, short b, short c, short *d);
void getAmplitudesArray(string *fstFileToCompare, string *secFileToCompare, int *channels);
void compareMono(vector<short> *fstArr, vector<short> *secArr);
void compareStereo(vector<short> *leftFstArr, vector<short> *rightFstArr, vector<short> *leftSecArr, vector<short> *rightSecArr);
void compareWAVfiles(string *fstFileToCompare, string *secFileToCompare);
//vector <short> readAmplitudesFromWAV(string fileName/*, vector <short> *vectorToAmplitudes*/);
vector <short> readAmplitudesFromWAV(string fileName, SF_INFO fileInfo);
//short readAmplitudesFromWAV(string fileName);


/* Функция, отвечающая за сравнение двух wav файлов.
На вход поступает две строки, которые являются названиями файлов. После их получения происходит 
их считывание в массив и последующая процедура вызова сравнения.
@fstFileToCompare и @secFileToCompare являются строками с названиями файла.
По итогу работы, в месте запуска программы создаётся файл compareWAV.dat с резульататми сравнения двух файлов. */
void compareWAVfiles(string *fstFileToCompare, string *secFileToCompare) {
	// Доработать многопоточность
	//vector <short> *fstAmplitudesArrayFromFile = new vector <short>;
	//vector <short> *secAmplitudesArrayFromFile = new vector <short>;
	//thread fst(readAmplitudesFromWAV, *fstFileToCompare, *fstAmplitudesArrayFromFile);
	//thread sec(readAmplitudesFromWAV, *secFileToCompare, *secAmplitudesArrayFromFile);
	//fst.join();
	//sec.join();
	// заранее делаем копии наших передаваемыхъ данных для удобства работы
	string fstFileName = *fstFileToCompare, secFileName = *secFileToCompare;
	SF_INFO fstFileInfo, secFileInfo;
	// Узнаём данные о WAV файле. В дальнейшем, можно будет и для большего количества файлов сделать.
	SNDFILE *fstFileWAV = sf_open(fstFileName.c_str(), SFM_READ, &fstFileInfo);
	SNDFILE *secFileWAV = sf_open(secFileName.c_str(), SFM_READ, &secFileInfo);
	int fstFilechannels = fstFileInfo.channels, secFilechannels = secFileInfo.channels;
	if (fstFilechannels != secFilechannels) {
		cout << "Количество каналов сравниваемых файлов не равно! В файлах:\n\t" <<
			fstFileName << " содержится " << fstFilechannels << " каналов.\n\t" <<
			secFileName << " содержится " << secFilechannels << " каналов.\n";
	}
	vector <short> fstArr = readAmplitudesFromWAV(*fstFileToCompare, fstFileInfo);
	vector <short> secArr = readAmplitudesFromWAV(*secFileToCompare, secFileInfo);
	if(fstFilechannels == 1) {
		compareMono(&fstArr, &secArr);
		delete &fstArr, &secArr;
	} else if (fstFilechannels == 2) {
		vector <short> leftFstArr, rightFstArr, leftSecArr, rightSecArr;
		bool channel = false;
		for (int i = 0; i < fstArr.size(); i++) {
			if (channel == false) {
				leftFstArr.push_back(fstArr.at(i));
				leftSecArr.push_back(secArr.at(i));
				channel == true;
			} else if (channel == true) {
				rightFstArr.push_back(fstArr.at(i));
				rightSecArr.push_back(secArr.at(i));
				channel == false;
			}
		}
		compareStereo(&leftFstArr, &rightFstArr, &leftSecArr, &rightSecArr);
		delete &leftFstArr, &rightFstArr, &leftSecArr, &rightSecArr;
	}
	sf_close(fstFileWAV);
	sf_close(secFileWAV);
	cout << "Сравнение файлов прошло успешно!\n";
}

/* Сама функция непосредственного считывания амплитуд из файла. Специально реализована как отдельная функция
для возможности реализации в дальнейшем многопоточности. 
На вход поступает @filename - название нашего файла в формате строки, @fileInfo - указатель на структуру данных
данного аудиофайла.
В самой же процедуре считывание файла происходит с помощью библиотеки "libsndfile". 
Руководство по данной библиотеке - http://www.mega-nerd.com/libsndfile/api.html */
vector <short> readAmplitudesFromWAV(string fileName, SF_INFO fileInfo/*, vector <short> vectorToAmplitudes*/ ) {
	SNDFILE *fileWAV;
	vector <short> vectorWithAmplitudes;
	if ((fileWAV = sf_open(fileName.c_str(), SFM_READ, &fileInfo)) = NULL) {
		cout << "Файл не найден! Работа прекращена";
		vector <short> nullvector;
		return vectorWithAmplitudes;
	} else {
		//int channels = fileInfo.channels, frames = fileInfo.frames;
		//short *arrayAmplitudes = new short[frames];
		short *buffer = new short[BLOCK_SIZE];
		int count, k = 0;
		while (count = static_cast<short>(sf_read_short(fileWAV, &buffer[0], BLOCK_SIZE)) > 0) {
			//vectorToAmplitudes.push_back(buffer[0]);
			vectorWithAmplitudes.push_back(buffer[0]);
			//arrayAmplitudes[k] = buffer[0];
			//k++;
		}
		sf_close(fileWAV);
		cout << "Файл " << fileName << " успешно прочитан!\n";
		//return *arrayAmplitudes;
		return vectorWithAmplitudes;
	}
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
	string compare = "compare", compareWAV = "compareWAV";
	if (argc == 0) {
		goto noARGS;
	} else if (argc > 1 && !compare.compare(argv[1])) {
		// для проверки compare list1.dat list2.dat 1
		string fstFileToCompare = argv[2];
		string secFileToCompare = argv[3];
		int channels = atoi(argv[4]);
		getAmplitudesArray(&fstFileToCompare, &secFileToCompare, &channels);
	} else if (argc > 1 && !compareWAV.compare(argv[1])) {
		// для проверки - compareWAV 11.wav "Coldplay - Viva La Vida (low).wav"
		string fstFileToCompare = argv[2];
		string secFileToCompare = argv[3];
		compareWAVfiles(&fstFileToCompare, &secFileToCompare);
	}
	noARGS:
	string fstFile = "", secFile = "";
	int choose = 0, channels = 0;
	cout << "Выберите сравнение\nДвух файлов с амплитудами - 1\nДвух WAV файлов - 2\nВыбор: ";
	cin >> choose;
	if (choose == 1) {
		cout << "Введите название первого файла для сравнения: ";
		getline(cin, fstFile);
		cout << "Введите название второго файла для сравнения: ";
		getline(cin, secFile);
		cout << "Укажите количество каналов аудио обоих файлов: ";
		cin >> channels;
		getAmplitudesArray(&fstFile, &secFile, &channels);
	} else if (choose == 2) {
		cout << "Введите название первого музыкального файла: ";
		getline(cin, fstFile);
		cout << "Введите название второго музыкального файла: ";
		getline(cin, secFile);
		getAmplitudesArray(&fstFile, &secFile, &channels);
	}
	return 0;

}

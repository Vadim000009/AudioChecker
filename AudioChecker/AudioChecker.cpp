#define _CRT_SECURE_NO_WARNINGS	// не видеть ругательств компилятора
#define ENABLE_SNDFILE_WINDOWS_PROTOTYPES 1
#define BLOCK_SIZE 2
#include <windows.h>
#include "sndfile.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <string>
#include <thread>
#include <stdlib.h>

using namespace std;

struct PRIMITIV {
	//uint16_t amplitude;	// Должен быть unsigned short
	uint8_t amplitude;
	uint8_t straight;
	uint8_t oblique;
	//PRIMITIV(uint16_t amplitude, uint8_t straight, uint8_t oblique) : amplitude(amplitude), straight(straight), oblique(oblique) {}
	PRIMITIV(uint8_t amplitude, uint8_t straight, uint8_t oblique) : amplitude(amplitude), straight(straight), oblique(oblique) {}
};

typedef struct {
	char     chunkId[4];
	uint32_t chunkSize;
	char     format[4];
	char     subchunk1Id[4];
	uint32_t subchunk1Size;
	uint16_t audioFormat;
	uint16_t numChannels;
	uint32_t sampleRate;
	uint32_t byteRate;
	uint16_t blockAlign;
	uint16_t bitsPerSample;
	char     subchunk2Id[4];
	uint32_t subchunk2Size;
} WavHeader;

//typedef unsigned char(__cdecl *Point) (unsigned int size, unsigned int adrArr, unsigned int sizeArr, unsigned int newArr, unsigned int sizeFinal, unsigned int waterMet);
void getAmplitudesArray(string *fstFileToCompare, string *secFileToCompare, int *channels);
void compareMono(vector<short> *fstArr, vector<short> *secArr);
void compareStereo(vector<short> *leftFstArr, vector<short> *rightFstArr, vector<short> *leftSecArr, vector<short> *rightSecArr);
void compareWAVfiles(string *fstFileToCompare, string *secFileToCompare);
void readAmplitudesFromWAV(string *fileName, SF_INFO fileInfo,  vector <short>& vectorToAmplitudes);
int getAmplitudesFromWavToTXT(string *fileName);
int createWAVfromPRIMITIV(string *fileName, string *serviceFileName);
int createFragments(string *fileName);
int getHeadingFrowWavFile(string *fileName);
int sintezFragments(string *fileName);
streampos fileSize(ifstream& file);

int getAmplitudesFromWavToTXT(string *fileName) {
	string fileNameToTXT = *fileName;
	SF_INFO fileInfo;
	SNDFILE *fileWAV = sf_open(fileName->c_str(), SFM_READ, &fileInfo);
	if (fileWAV == NULL) {
		cout << "Данный файл не существует или недоступен! Программа будет прекращена\n";
		return -1;
	}
	vector <short> arrayWAV;
	readAmplitudesFromWAV(fileName, fileInfo, arrayWAV);
	fileNameToTXT.insert(fileName->length() - 3, "dat");
	fileNameToTXT.erase(fileNameToTXT.length() - 3, fileNameToTXT.length());
	ofstream fileWithAmplitudes(fileNameToTXT.c_str(), ios::out);
	if (fileWithAmplitudes.is_open()) {
		for (long i = 0; i < (long) arrayWAV.size(); i++) {
			fileWithAmplitudes << arrayWAV.at(i) << endl;
		}
	}
	return 0;
}

/* Функция, отвечающая за сравнение двух wav файлов.
На вход поступает две строки, которые являются названиями файлов. После их получения происходит 
их считывание в массив и последующая процедура вызова сравнения.
@fstFileToCompare и @secFileToCompare являются строками с названиями файла.
По итогу работы, в месте запуска программы создаётся файл compareWAV.dat с резульататми сравнения двух файлов. */
void compareWAVfiles(string *fstFileToCompare, string *secFileToCompare) {
	SF_INFO fstFileInfo, secFileInfo;
	// Узнаём данные о WAV файле. В дальнейшем, можно будет и для большего количества файлов сделать.
	SNDFILE *fstFileWAV = sf_open(fstFileToCompare->c_str(), SFM_READ, &fstFileInfo),
		*secFileWAV = sf_open(secFileToCompare->c_str(), SFM_READ, &secFileInfo);
	if (fstFileWAV == NULL || secFileWAV == NULL) {
		cout << "Один из файлов указан НЕВЕРНО! Программа будет прекращена\n";
		return;
	}
	int fstFileChannels = fstFileInfo.channels, secFileChannels = secFileInfo.channels,
		fstFileFrames = fstFileInfo.frames, secFileFrames = secFileInfo.frames;
	if (fstFileChannels != secFileChannels) {
		cout << "Количество каналов сравниваемых файлов не равно! В файлах:\n\t" << 
			fstFileToCompare << " содержится " << fstFileChannels << " каналов.\n\t" <<
			secFileToCompare << " содержится " << secFileChannels << " каналов.\n";
		if (fstFileFrames < secFileFrames) {
			cout << "Амплитуд в файле " << secFileToCompare << " больше чем в " << fstFileToCompare << ". Программа будет прекращена.";
			return;
		}
	}
	vector <short> fstArr;
	vector <short> secArr;
	thread fst(readAmplitudesFromWAV, fstFileToCompare, fstFileInfo, ref(fstArr));
	thread sec(readAmplitudesFromWAV, secFileToCompare, secFileInfo, ref(secArr));
	fst.join();
	sec.join();
	if (fstFileChannels == 1) {
		compareMono(&fstArr, &secArr);
	} else if (fstFileChannels == 2) {
		vector <short> leftFstArr, rightFstArr, leftSecArr, rightSecArr;
		bool channel = false;
		for (long i = 0; i < (long) fstArr.size(); i++) {
			if (channel == false) {
				leftFstArr.push_back(fstArr.at(i));
				leftSecArr.push_back(secArr.at(i));
				channel = true;
			} else if (channel == true) {
				rightFstArr.push_back(fstArr.at(i));
				rightSecArr.push_back(secArr.at(i));
				channel = false;
			}
		}
		compareStereo(&leftFstArr, &rightFstArr, &leftSecArr, &rightSecArr);
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
void readAmplitudesFromWAV(string *fileName, SF_INFO fileInfo, vector <short>& vectorToAmplitudes) {
	SNDFILE *fileWAV = sf_open(fileName->c_str(), SFM_READ, &fileInfo);
	if (fileWAV == NULL) {
		cout << "Файл не найден! Работа прекращена";
		return;
	} else {
		byte *buffer = new byte[BLOCK_SIZE];
		int count, k = 0;
		while (count = static_cast<byte>(sf_read_raw(fileWAV, &buffer[0], BLOCK_SIZE)) > 0) {
			vectorToAmplitudes.push_back(buffer[0]);
		}
		sf_close(fileWAV);
		cout << "Файл " << fileName << " успешно прочитан!\n";
	}
}

/* Функция, получающая амплитуды из двух файлов и загоняющая их в векторный массив.
@fstFileToCompare и @secFileToCompare являются указателями на файлы,из которых будут полученны амплитуды.
@channels является указателем на количество каналов аудио. 
Возможны случаи получения для моно и стерео. */
void getAmplitudesArray(string *fstFileToCompare, string *secFileToCompare, int *channels) {
	ifstream fstOpen, secOpen;
	fstOpen.open(fstFileToCompare->c_str(), ios_base::in);
	secOpen.open(secFileToCompare->c_str(), ios_base::in);
	short amplitude = 0;
	if (!(fstOpen.is_open() && secOpen.is_open())) {
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
			". Количество амплитуд второго файла: " << monoChannelSecFile.size() << "\n";
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
			try {
				if (fstArr->at(i) != secArr->at(i)) {
					fileResult << "Позиция\t" << i << " значения\t" << fstArr->at(i) << "\tи\t" << secArr->at(i) << endl;
					count++;
				}
			} catch (std::out_of_range e) {
				cout << "\nВыход за границы массива\n";
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


/* Узнаём длину файла */
std::streampos fileSize(ifstream& file) {
	std::streampos fsize = 0;
	fsize = file.tellg();
	file.seekg(0, std::ios::end);
	fsize = file.tellg() - fsize;
	file.seekg(0, std::ios::beg);
	return fsize;
}

/*Функция синтеза из S и K файла
Требуется dll DFEN или FENIKS*/
int createWAVfromPRIMITIV(string *fileName, string *serviceFileName) {
	cout << "Считывание файла с ПРИМИТИВами из '" << *fileName << "' \n";
	ifstream readSFile(*fileName, ios::binary);				// Открываем файл в бинарном представлении
	vector <PRIMITIV> doubleBytesFromFile;					// Создаём вектор со структурой примитива
	if (readSFile.is_open()) {								// Если файл открыт
		long length = fileSize(readSFile);					// Узнаём размерность файла
		char * buffer = (char*)malloc(length * sizeof(int8_t));
		readSFile.read(buffer, length);						// Считываем файл

		//uint16_t amplitude;								// Должен быть на 16 байт
		uint8_t amplitude;									// Так как сейчас работаем на 8 байт
		uint8_t straight, oblique;							// Из условия
		for (int i = 0; i < length; i = i + 4) {			// начинаем записывать массив в структуру
			amplitude = (buffer[i + 1] << 8) + buffer[i];	// Сдвиг на 8 бит для записи хвостика (в случае 1 байта бесполезен) (читывать информацию необходимо задом наперёд, т.е. вначале второе, потом первое)
			//amplitude = buffer[i];							// считываем 8 байт
			straight = buffer[i + 2];						// Записываем количество отсчётов по прямой
			oblique = buffer[i + 3];						// Записываем количество отсчётов по косой
			doubleBytesFromFile.push_back(PRIMITIV(amplitude, straight, oblique));	// Записываем полученную структуру в вектор
		} 
		free(buffer);										// Удаление буфера, так как всё в векторе
	} else {
		cout << "Ошибка в названии файла! Выход из программы\n";
		return 1;
	}
	readSFile.close();										// Закрываем файл

	typedef void(__cdecl *Point) (uint32_t amplitudeFst, uint32_t amplitudeSec, uint32_t samples, uint16_t *arrayResult);	// Прототип процедуры
	HMODULE handle = LoadLibrary(L"dfen.dll");							// Ищем DLL в папке с программой
	Point procAmplitudes = (Point)GetProcAddress(handle, "F@enik");		// Загружаем адрес процедуры
	//vector <uint16_t> amplits;										// Создаём результирующий вектор
	vector <uint8_t> amplits;											// Создаём результирующий вектор
	if (procAmplitudes != NULL) {
		for (long i = 0; i <= (long)doubleBytesFromFile.size() - 2; i++) { // Отсчёт идёт с 0, а при -1 всё равно идёт заход на size()
			amplits.push_back(doubleBytesFromFile[i].amplitude);			// Добавляем первую амплитуду 
			//00 – пусто не используется
			//01 – отсутствует первый параметр отсчеты только от  S1 до  S3;
			//10 – предшествующий параметр рассматривается как S1max=S2min  и  наоборот 	
			// S1min =  S2max, а количество отсчетов  определяет расстояние между ними 
			//11 - S1max = S2max    или  S1min = S2min
			if (static_cast<int> (doubleBytesFromFile[i].oblique) >= 1) {								// Если что-то то есть по косой от 1 и более
				if (static_cast<int> (doubleBytesFromFile[i].straight) >= 1) {							// И что-то есть по прямой от 1 и более
					for (int j = 0; j < static_cast<int> (doubleBytesFromFile[i].oblique); j++) {		// Тогда пишем косой
						amplits.push_back(doubleBytesFromFile[i].amplitude);
					}
				} else if (static_cast<int> (doubleBytesFromFile[i].oblique) > 1) {						// Иначе если более 1 по косой
					for (int j = 0; j < static_cast<int> (doubleBytesFromFile[i].oblique) - 1; j++) {	// Пишем по косой с учётом одного вычета
						amplits.push_back(doubleBytesFromFile[i].amplitude);
					}
				}
			}
			if (static_cast<int> (doubleBytesFromFile[i].straight) > 1) {								// Если более 1 по прямой
				uint8_t posFst = doubleBytesFromFile[i].amplitude, posSec = doubleBytesFromFile[i + 1].amplitude; // должен быть uint16_t
				uint8_t samples = doubleBytesFromFile[i].straight;
				if (static_cast<int> (samples) > 2 && posFst + 1 != posSec && posSec + 1 != posFst && posSec != posFst) {
					uint16_t* finalAmp = new uint16_t[samples + 1]{ 0 };
					procAmplitudes(posFst, posSec, samples, &finalAmp[0]);
					for (int j = 0; j < samples - 1; j++) {
						amplits.push_back(finalAmp[j]);
					}
					delete[] finalAmp;
				}
				else if (static_cast<int> (samples) == 2 || posFst + 1 == posSec || posSec + 1 == posFst || posSec == posFst) {
					for (int i = 0; i < samples; i++) {
						amplits.push_back(round((posFst + posSec) / 2));
					}
				}
			}
		}
		amplits.push_back(doubleBytesFromFile[doubleBytesFromFile.size() - 1].amplitude);				// Записываем последнюю амплитуду
	} else {
		cout << "Ошибка хандла dll. Проверьте наличие dll в папке (dfen.dll)\n";
		return 1;
	}
	FreeLibrary(handle);													// Высвобождаем библиотеку

	cout << "Считывание заголовка из " << *serviceFileName << "\n";			
	ifstream fileK1(*serviceFileName, ios::binary);							// Открываем бинарный файл
	vector <uint8_t> K1;													// Итоговый вектор с файлом
	if (fileK1.is_open()) {													// Если открыт
		long length = fileSize(fileK1);										// Узнаём размер
		char * buffer = new char[length];									// Считываем его в массив чаров
		fileK1.read(buffer, length);										// Считали указанную длину
		for (long i = 0; i < length; i++) {
			K1.push_back(buffer[i]);										// Сохранили его в вектор
		}
	} else {
		cout << "Ошибка в открытии файла " << *serviceFileName << "\n";
		return 1;
	}
	fileK1.close();															// Закрыли файл

	K1.insert(K1.end(), amplits.begin(), amplits.end());					// Вставили в вектор с заголовком амплитуды
	FILE* fileWav = fopen("sintezPrimitiv.wav", "wb");						// Создали итоговый файл
	if (fileWav != NULL) {													// Если удалось
		fwrite(K1.data(), sizeof(uint8_t), K1.size(), fileWav);				// Записали вектор в файл
		fclose(fileWav);													// Закрыли файл
		cout << "Данные успешно записаны в файл.\n";
	} else {
		cout << "Не удалось открыть файл для записи.\n";
		return 1;
	}
	cout << "Успех! Результат в файле 'sintezPrimitiv.wav'\n";
}

// Запустить питон и передать как параметр два файла
// Ожидать закрытия питона
// PS - py .\Graphs.py t1.dat S1+K1.dat
void getGraphsFromFile(string *fstFile, string *secFile) {
	string command = "cmd /C py Graphs.py ";
	command.append(*fstFile).append(" ").append(*secFile);
	system(command.c_str());
}

/*
Функция для работы с dll фрагментации.
На вход поступает ссылка на наименование файла или его абсолютный путь, если файл находится не в директории программы.
Для работы необходима библиотека Fragm.
*/
int createFragments(string *fileName) {
	cout << "Считывание файла '" << *fileName << "' во фрагментации\n";
	typedef int(__cdecl *Point) (
		unsigned int amplitudeSize,		// Значение амплитуды (число 1/2/4)
		char *arrayWithSFile,			// Указатель на S массив из файла в бинарном виле
		unsigned int arraySSize,		// Размер массива S 
		uint8_t *arrayForFFile,			// Указатель на F массив, в который будет записан итоговый массив
		unsigned int arrayFSize);		// Размер массива (должен быть в два раза больше S)

	ifstream file(*fileName, ios::binary);								// Открываем считываение бинарного файла
	long length = 0;													// Длина понадобится в дальнейшем
	char* arraySfile;													// Делаем указатель на массив с S файлом
	if (file.is_open()) {												// Если файл открыт
		length = fileSize(file);										// Узнаём размерность файла
		arraySfile = (char*)malloc(length * sizeof(uint8_t));			// Создаём массив под данные
		file.read(arraySfile, length);									// считываем в массив с динамической памятью
	} else {
		cout << "Ошибка в названии файла! Выход из программы\n";
		return 1;
	}
	file.close();														// освобождаем файл
	/*	for (int i = 0; i < length; ++i)
	{
		cout << i << ": " << (unsigned short)arraySfile[i] << endl;
	}*/
	length = length * 3;//length * 2 + 32 + 4;										// Цыганские фокусы (длина на х2 и две информационные строки + заключительный описатель)
	uint8_t * arrayFfile = (uint8_t*)malloc(length);					// Создаём массив для F файла в два раза больше S
	memset(arrayFfile, 0, length);									// Заполняем массив нулями для корректности
	HMODULE handleFragm = LoadLibrary(L"FRAGM.dll");					// Ищем DLL в папке с программой
	Point fragmentDll = (Point)GetProcAddress(handleFragm, "F@RAGM");	// Указываем адрес процедуры для передачи параметров
	long size = 0;
	if (fragmentDll != NULL) {											// Если DLL есть
		size = fragmentDll(2, arraySfile, (unsigned int)length, arrayFfile, (unsigned int)length);
	} else {
		cout << "Нет хандла FRAGM.dll\n";
		return 1;
	}
	FreeLibrary(handleFragm);											// Освобождаем DLL от хандла
	free(arraySfile);													// Высвобождаем память от массива S файла

	FILE* fragFile = fopen("frag.txt", "wb");							// Создаём файл
	if (fragFile != NULL) {												// Если удалось создать
		if (size == (unsigned long)-1) {
			// не помню код ошибки
		} else if (size == (unsigned long) -2) {
			cout << "Файл имеет плохую структуру (0 по прямой, 0 по косой)\n";
		} else {
			length = size;												// Устанавливаем размер массива, так как он пришёл без ошибок
		}
		fwrite(arrayFfile, sizeof(int8_t), length, fragFile);			// Записываем массив с размером Int_8t и длиной в файл
		fclose(fragFile);												// Закрываем файл
	} else {
		cout << "Не удалось открыть файл для записи.\n";
		return 1;
	}
	free(arrayFfile);													// Осовобождаем память из массива
	return 0;
}

int sintezFragments(string *fileName) {
	cout << "Считывание файла '" << *fileName << "' с фрагментами\n";
	typedef int(__cdecl *Point) (
		unsigned int amplitudeSize,		// Значение амплитуды (число 1/2/4)
		char *arrayWithFFile,			// Указатель на S массив из файла в бинарном виле
		int arrayFSize,		// Размер массива S 
		uint8_t *arrayForSFile,			// Указатель на F массив, в который будет записан итоговый массив
		int arraySSize);		// Размер массива (должен быть в два раза больше S)
	ifstream fileFrags(*fileName, ios::binary);							// Открываем считываение бинарного файла
	long length = 0;													// Длина понадобится в дальнейшем
	char* arrayFfile;													// Делаем указатель на массив с S файлом
	if (fileFrags.is_open()) {											// Если файл открыт
		length = fileSize(fileFrags);									// Узнаём размерность файла
		arrayFfile = (char*)malloc(length * sizeof(uint8_t));			// Создаём массив под данные
		fileFrags.read(arrayFfile, length);								// считываем в массив с динамической памятью
	} else {
		cout << "Ошибка в названии файла! Выход из программы\n";
		return 1;
	}
	fileFrags.close();

	uint8_t * arraySfile = (uint8_t*)malloc(length);				// Создаём массив для F файла в два раза больше S
	memset(arrayFfile, 0, length);									// Заполняем массив нулями для корректности
	HMODULE handleSintezFragm = LoadLibrary(L"comfra.dll");					// Ищем DLL в папке с программой
	Point defragmentDll = (Point)GetProcAddress(handleSintezFragm, "D@ECOMF");	// Указываем адрес процедуры для передачи параметров
	long size = 0;
	if (defragmentDll != NULL) {											// Если DLL есть
		size = defragmentDll(1, arrayFfile, (int)length, arraySfile, (int)length);
	} else {
		cout << "Нет хандла Comfra.dll\n";
		return 1;
	}

	// выполнить передачу данных в dll
	FreeLibrary(handleSintezFragm);											// Освобождаем DLL от хандла
	free(arrayFfile);														// Высвобождаем память от массива S файла
	
	FILE* sintezFFile = fopen("s_file.txt", "wb");							// Создаём файл
	if (sintezFFile != NULL) {												// Если удалось создать
		fwrite(arraySfile, sizeof(int8_t), length, sintezFFile);			// Записываем массив с размером Int_8t и длиной в файл
		fclose(sintezFFile);												// Закрываем файл
	}
	else {
		cout << "Не удалось открыть файл для записи.\n";
		return 1;
	}
	free(arraySfile);														// Осовобождаем память из массива
	return 0;
}

/*
Функция для получения заголовка файла
На вход поступает наименование wav файла, который должен быть открыт и из которого будет извелён заголовок.
*/
int getHeadingFrowWavFile(string *fileName) {
	FILE* fileWithHeader = fopen(fileName->c_str(), "rb");				// Считываем файл
	WavHeader header;													// Создаём структуру. куда считываем
	if (fileWithHeader != NULL) {
		fread(&header, sizeof(WavHeader), 1, fileWithHeader);			// Чтение заголовка файла WAV
		fclose(fileWithHeader);											// Закрываем файл
		cout << "Файл успешно прочитан\n";
	} else {
		cout << "Ошибка открытия файла\n";
		return 1;
	}
	
	FILE* headerFile = fopen(fileName->append(".h_uni").c_str(), "wb");	// Создаём файл
	if (headerFile != NULL) {											// Если удалось создать
		fwrite(&header, sizeof(int8_t), sizeof(header), headerFile);	// Записываем массив с размером Int_8t и длиной в файл
		fclose(headerFile);												// Закрываем файл
		cout << "файл " + *fileName + ".h_uni успешно записан\n";
	} else {
		cout << "Не удалось открыть файл для записи.\n";
		return 1;
	}
}

int main(int argc, char* argv[]) {
	setlocale(LC_ALL, "rus");
	string compare = "/c", compareWAV = "/cwav", amplitudes = "/amp", primitiv = "/p";
	if (argc <= 1) {
		string fstFile = "", secFile = "", choose = "", channels = "";
		start:
		cout << "Выберите действие\nДвух файлов с амплитудами - 1\n" 
			"Двух WAV файлов - 2\n" 
			"Получить амплитуды из файла WAV - 3\n"
			"Восстановить амплитуды из указанного файла - 4\n"
			"Посмотреть графики (Требуется Python) - 5\n"
			"Считать заголовок файла - 6\n"
			"Получить фрагменты файла - 7\n"
			"Восстановить из фрагментов структуры - 8\n"
			"Выбор: ";
		getline(cin, choose);
		if (choose == "1") {
			cout << "Введите название первого файла для сравнения: ";
			getline(cin, fstFile);
			cout << "Введите название второго файла для сравнения: ";
			getline(cin, secFile);
			cout << "Укажите количество каналов аудио обоих файлов: ";
			getline(cin, channels);
			int channel = atoi(channels.c_str());
			fstFile = "source.dat";
			secFile = "sintezPrimitiv.dat";
			getAmplitudesArray(&fstFile, &secFile, &channel);
		} else if (choose == "2") {
			cout << "Введите название первого музыкального файла: ";
			getline(cin, fstFile);
			cout << "Введите название второго музыкального файла: ";
			getline(cin, secFile);
			compareWAVfiles(&fstFile, &secFile);
		} else if (choose == "3") {
			cout << "Введите название музыкального файла типа WAV: ";
			getline(cin, fstFile);
			getAmplitudesFromWavToTXT(&fstFile);
		} else if (choose == "4") {
			cout << "Укажите название файла структур: ";
			getline(cin, fstFile);
			cout << "Укажите название файла, содержащий служебную информацию: ";
			getline(cin, secFile);
			createWAVfromPRIMITIV(&fstFile, &secFile);
		} else if (choose == "5") {
			cout << "Укажите название певрого файла: ";
			getline(cin, fstFile);
			cout << "Укажите название второго файла: ";
			getline(cin, secFile);
			getGraphsFromFile(&fstFile, &secFile);
		} else if (choose == "6") {
			cout << "Укажите название файла, их которого необходимо извлечь заголовок: ";
			getline(cin, fstFile);
			getHeadingFrowWavFile(&fstFile);
		} else if (choose == "7") {
			cout << "Укажите файл, который необходимо фрагментировать: ";
			getline(cin, fstFile);
			createFragments(&fstFile);
		} else if (choose == "8") {
			cout << "Укажите файл, из которого необходимо восстановить\nфрагментированную ифнормацию: ";
			getline(cin, fstFile);
			sintezFragments(&fstFile);
		} else {
			return 0;
		}
		system("pause");
		system("cls");
		goto start;
	} else {
		if (argc > 1 && !compare.compare(argv[1])) {							// для проверки - /c list1.dat list2.dat
			string fstFileToCompare = argv[2];
			string secFileToCompare = argv[3];
			int channels = atoi(argv[4]);
			getAmplitudesArray(&fstFileToCompare, &secFileToCompare, &channels);
		} else if (argc > 1 && !compareWAV.compare(argv[1])) {					// для проверки - /cwav 11.wav "Coldplay - Viva La Vida (low).wav"
			string fstFileToCompare = argv[2];
			string secFileToCompare = argv[3];
			compareWAVfiles(&fstFileToCompare, &secFileToCompare);
		} else if (argc > 1 && !amplitudes.compare(argv[1])) {					// для проверки - /amp s1.wav
			string fileName = argv[2];
			getAmplitudesFromWavToTXT(&fileName);
		} else if (argc > 1 && !primitiv.compare(argv[1])) {					// для проверки - /p s1.txt k1.txt
			string filewithPrimitivs = argv[2];
			string serviceFile = argv[3];
			createWAVfromPRIMITIV(&filewithPrimitivs, &serviceFile);
		}
		return 0;
	}
}

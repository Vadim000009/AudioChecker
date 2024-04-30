#define _CRT_SECURE_NO_WARNINGS	// не видеть ругательств компилятора
#define ENABLE_SNDFILE_WINDOWS_PROTOTYPES 1
#include <windows.h>
#include "sndfile.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <string>
#include <thread>
#include <stdlib.h>
#include <map>
#include <queue>

using namespace std;

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

void getFilesWithAmplitudes(string *fileFirstName, string *fileSecondName, int *channels, bool* mode);
void compareRAWData(uint8_t* fileFirstCompare, int fileFirstCompareSize, string fileFirstCompareName, uint8_t *fileSecondCompare, string fileSecondCompareName, int *channels, bool *mode);
void comparatorArrayRAW(uint8_t *arrayFirst, uint8_t *arraySecond, ofstream &fileCompare, int *arraySize, int cycleStart);
void compareMono(vector<short> *fstArr, vector<short> *secArr);
void compareStereo(vector<short> *leftFstArr, vector<short> *rightFstArr, vector<short> *leftSecArr, vector<short> *rightSecArr);
void compareWAVfiles(string *fstFileToCompare, string *secFileToCompare);
void readAmplitudesFromWAVTypeUnsignedByte(string *fileName, SF_INFO fileInfo, vector <uint8_t>& vectorToAmplitudes);
void readAmplitudesFromWAV(string *fileName, SF_INFO fileInfo, vector <uint8_t>& vectorToAmplitudes);
int getFileWithAmplitudesToText(string *fileName, bool *mode);
int sintezWavFromUNIPRIM(string *fileName, string *serviceFileName, bool debugInfo);
int createFragments(string *fileName);
int getHeadingFrowWavFile(string *fileName);
int sintezFragments(string *fileName);
streampos fileGetSize(ifstream& file);
int createFramesFromWAV(string *fileName);
uint8_t* fileReadRAW(string *fileName, int* fileSize);

int getFileWithAmplitudesToText(string *fileName, bool *mode) {
	string fileResultName;
	uint8_t* arrayAmplitudes;
	WavHeader wav;
	if (mode) fileResultName = *fileName + ".wdata";
	else fileResultName = "wavtxt.wdata";
	ifstream fileAmplitudes(*fileName, ios::in | ios::binary);
	if (fileAmplitudes.is_open()) {
		fileAmplitudes.read((char*)&wav, sizeof(WavHeader));
		cout << "Считывание файла " << *fileName << "\n";
		arrayAmplitudes = new uint8_t[wav.chunkSize];
		for (int i = 0; i < wav.chunkSize; i++)
			fileAmplitudes.read((char*)&arrayAmplitudes[i], 1);
	} else {
		cout << "Не удалось открыть файл " << fileName << "\n";
		return -1;
	}
	ofstream fileResult(fileResultName, ios::out);
	if (fileResult.is_open()) {
		for (int i = 0; i < wav.chunkSize; i++)
			fileResult.write((char*)arrayAmplitudes[i], 1); //"%o\n"
		cout << "Данные записаны в файл " << fileResultName << "\n";
	} else {
		cout << "Не удалось создать файл\n";
	}

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
		for (long i = 0; i < (long)arrayWAV.size(); i++) {
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
	if (fstFileInfo.channels != secFileInfo.channels) {
		cout << "Количество каналов сравниваемых файлов не равно! В файлах:\n\t" <<
			fstFileToCompare << " содержится " << fstFileInfo.channels << " каналов.\n\t" <<
			secFileToCompare << " содержится " << secFileInfo.channels << " каналов.\n";
		if (fstFileInfo.frames < secFileInfo.frames) {
			cout << "Амплитуд в файле " << secFileToCompare << " больше чем в " << fstFileToCompare << ". Программа будет прекращена.";
			return;
		}
	}
	vector <uint8_t> fstArr;
	vector <uint8_t> secArr;
	thread fst(readAmplitudesFromWAV, fstFileToCompare, fstFileInfo, ref(fstArr));
	thread sec(readAmplitudesFromWAV, secFileToCompare, secFileInfo, ref(secArr));
	fst.join();
	sec.join();
	if (fstFileInfo.channels == 1) {
		compareMono(&fstArr, &secArr);
	} else if (fstFileInfo.channels == 2) {
		vector <short> leftFstArr, rightFstArr, leftSecArr, rightSecArr;
		bool channel = false;
		for (long i = 0; i < (long)fstArr.size(); i++) {
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
void readAmplitudesFromWAV(string *fileName, SF_INFO fileInfo, vector <uint8_t>& vectorToAmplitudes) {
	SNDFILE *fileWAV = sf_open(fileName->c_str(), SFM_READ, &fileInfo);
	if (fileWAV == NULL) {
		cout << "Файл не найден! Работа прекращена";
		return;
	} else {
		uint8_t *buffer = new uint8_t[1];
		int count = 0;
		while (count = static_cast<uint8_t>(sf_read_raw(fileWAV, &buffer[0], 1)) > 0) {
			vectorToAmplitudes.push_back(buffer[0]);
		}
		sf_close(fileWAV);
		cout << "Файл " << *fileName << " успешно прочитан!\n";
	}
}

void readAmplitudesFromWAVTypeUnsignedByte(string *fileName, SF_INFO fileInfo, vector <uint8_t>& vectorToAmplitudes) {
	SNDFILE *fileWAV = sf_open(fileName->c_str(), SFM_READ, &fileInfo);
	if (fileWAV == NULL) {
		cout << "Файл не найден! Работа прекращена";
		return;
	} else {
		uint8_t *buffer = new uint8_t[1];
		int count;
		while (count = static_cast<uint8_t>(sf_read_raw(fileWAV, &buffer[0], 1)) > 0) {
			vectorToAmplitudes.push_back(buffer[0]);
		}
		sf_close(fileWAV);
		cout << "Файл " << *fileName << " успешно прочитан!\n";
	}
}

/* Функция, получающая амплитуды из двух файлов и переводит их в два массива, которые в последующем сравниваются.
@fileFirstName и @fileSecondName являются указателями на файлы,из которых будут полученны амплитуды.
@channels является указателем на количество каналов аудио, @mode является указанием на то, в каком режиме получается файл. */
void getFilesWithAmplitudes(string *fileFirstName, string *fileSecondName, int *channels, bool *mode) {
	int fileFirstSize = 0, fileSecondSize = 0;
	uint8_t* fileFirstRAW = fileReadRAW(fileFirstName, &fileFirstSize);
	if (fileFirstRAW == NULL) return;
	uint8_t* fileSecondRAW = fileReadRAW(fileSecondName, &fileSecondSize);
	cout << "Чтение прошло успешно!\nКоличество амплитуд первого файла: " << fileFirstSize << "\nКоличество амплитуд второго файла: " << fileSecondSize << "\n";
	if (fileFirstSize != fileSecondSize) {
		cout << "Файлы не совпадают по количеству амплитуд\n";
		return;
	}
	compareRAWData(fileFirstRAW, fileFirstSize, *fileFirstName, fileSecondRAW, *fileSecondName, channels, mode);
}

void compareRAWData(uint8_t* fileFirstCompare, int fileFirstCompareSize, string fileFirstCompareName, uint8_t *fileSecondCompare, string fileSecondCompareName, int *channels, bool *mode) {
	string fileResultCompareName;
	double fileResultComparePercent = 0;
	int count = 0;

	if (*mode)	fileResultCompareName = "ResultCompare.cdat";
	else fileResultCompareName = fileFirstCompareName + "_CompareWith_" + fileSecondCompareName + ".cdat";

	ofstream fileResultCompare(fileResultCompareName, ios::app | ios::out);
	if (fileResultCompare.is_open()) {
		if (*channels == 1) {
			comparatorArrayRAW(fileFirstCompare, fileSecondCompare, fileResultCompare, &fileFirstCompareSize, 0);
		} else if (*channels == 2) {
			fileResultCompare << "Сравнение канала 1\n";
			comparatorArrayRAW(fileFirstCompare, fileSecondCompare, fileResultCompare, &fileFirstCompareSize, 0);
			fileResultCompare << "Сравнение канала 2\n";
			comparatorArrayRAW(fileFirstCompare, fileSecondCompare, fileResultCompare, &fileFirstCompareSize, 2);
		}
	}
	else {
		cout << "Ошибка создания файла!";
	}
	fileResultCompare.close();
}

/* Функция сравнения двух массивов. На переданный указатель файла сразу же записывает результат с результатом в виде процентного соотношения.
@arrayFirst и @arraySecond являются указателями на первый и второвый массив со значениями.
@fileCompare является указателем на результирующий файл.
@arraySize и @delta являются указателями размерность массива и шаг цикла (0 или 2). */
void comparatorArrayRAW(uint8_t *arrayFirst, uint8_t *arraySecond, ofstream &fileCompare, int *arraySize, int delta) {
	double fileResultComparePercent = 0, arraySizeToCount = 0, count = 0;								// Объявляем переменные

	if (delta > 1) arraySizeToCount = *arraySize / delta;												// Если цикл начинается 2, то делим длину на два
	else arraySizeToCount = *arraySize;																	// Иначе указатель на полный размер

	for (long i = 0; i < (long)*arraySize; i = i + delta) {												// Цикл с условием шага из параметров
		if (arrayFirst[i] != arraySecond[i]) {															// Если не равны - записываем
			fileCompare << "Позиция\t" << i << " значение\t" << arrayFirst[i] << "\tи\t" << arraySecond[i] << "\n";
			count++;																					// Итерируем счётчик
		}
	}
	fileResultComparePercent = (100 / arraySizeToCount) * count;										// Вычисляем разницу по среднему арифметическому
	fileCompare << "По результатам сравнения, различие составило " << setprecision(3) << fileResultComparePercent << "%" << "\n";
}

/* Функция сравнения амплитуд в моно режиме. Создаёт файл resultMono.dat с результатами сравнения и указанием различия в определённых точках,
а так же в процентном соотношении об их разнице.
Кандидат на удаление
@fstArr и @secArr являются указателями на массивы векторов со значениями амплитуд. */
void compareMono(vector<uint8_t> *fstArr, vector<uint8_t> *secArr) {
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
Кандидат на удаление
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

uint8_t* fileReadRAW(string *fileName, int *fileSize) {
	ifstream fileRAW(*fileName, ios::binary);				// Открываем файл в бинарном представлении
	*fileSize = fileGetSize(fileRAW);						// Задаём длину считываемого файла 
	uint8_t* fileRead = new uint8_t[*fileSize];				// Создаем массив считываемого файла
	if (fileRAW.is_open()) {								// Если файл есть
		fileRAW.read((char*)fileRead, *fileSize);			// Считываем весь файл согласно его длине
		fileRAW.close();									// Закрываем файл
		return fileRead;
	}
	else {
		cout << "Ошибка в названии файла " << *fileName << "\n";
		return NULL;
	}
}

/*Функция синтеза из S и K файла
Требуется dll DFEN или FENIKS*/
int createWAVfromPRIMITIV(string *fileStructuresName, string *fileHeadingName, bool debugInfo) {
	string fileDebug = "degug_sintez_" + *fileStructuresName + ".txt";
	LPCWSTR DLLName = L"FenVod.dll";																	// Имя DLL
	LPCSTR DLLProcedureName = "F@enVod";																// Имя процедуры в DLL
	string fileOutputName = "sintezPrimitiv.wav";														// Имя файла, в который будет записан результат
	int fileStructuresSize = 0, fileSintezSize = 2000, fileHeadingSize = 0;								// Переменные с длинами тех или иных файлов

	cout << "Считывание файла с ПРИМИТИВами из '" << *fileStructuresName << "' \n";						// -- Считыванием файла со структурами --
	uint8_t* fileStructures = fileReadRAW(fileStructuresName, &fileStructuresSize);						// Процесс считывания
	if (fileStructures == NULL) return -1;
	for (long i = 0; i < fileStructuresSize; i = i + 4) {												// Цикл для высчитывания результирующего массива
		UINT8 temporaryNumber = 1 + (uint8_t)fileStructures[i + 2] + (uint8_t)fileStructures[i + 3];	// Считываем первую амплитуду + по прямой + по косой
		fileSintezSize = fileSintezSize + temporaryNumber;												// Итоговая переменная, в которую записывается суммированный результат
	}

	cout << "Считывание заголовка из " << *fileHeadingName << "\n";										// -- Считыванием файла с заголовком --
	uint8_t*  fileHeading = fileReadRAW(fileHeadingName, &fileHeadingSize);								// Процесс считывания
	if (fileHeading == NULL) return -1;

	//typedef int(__cdecl * Point) (uint8_t* amplitudeSize, uint8_t *fileWithStructures, UINT32 *fileWithStructuresSize, uint8_t* arrayForResult, UINT32 *arrayFroResultSize);	// Прототип процедуры
	//typedef int(__cdecl * Point) (uint8_t *amplitudeSize, uint8_t *fileWithStructures, uint32_t fileWithStructuresSize, uint8_t* arrayForResult, uint32_t arrayFroResultSize, int *wm);	// Прототип процедуры
	typedef int(__cdecl * Point) (uint8_t *amplitudeSize, uint8_t *fileWithStructures, int *fileWithStructuresSize, uint8_t* arrayForResult, int *arrayFroResultSize, int *wm);	// Прототип процедуры
	HMODULE DLLHandle = LoadLibrary(DLLName);															// Ищем DLL в папке с программой
	Point DLLProcedure = (Point)GetProcAddress(DLLHandle, DLLProcedureName);							// Загружаем адрес процедуры
	uint8_t* fileSintez = new uint8_t[fileSintezSize]{ 0 };												// Создаем результирующйи массив с величиной, высчитанной в цикле
	if (DLLProcedure != NULL) {																			// -- Процесс синтеза из структур --
		uint8_t amplitudeSize = 1;	// Если 11 - получаем количество возможных бит для водяного знака	// Задаём величину амплитуды (1,2,4)
		int wm = 1;
		uint8_t fileSintez[100]{ 0 };
		//int response = procAmplitudes(amplitudeSize, fileStructures, fileStructuresSize, fileSintez, fileSintezSize);
		//uint16_t response = DLLProcedure(&amplitudeSize, &fileStructures[0], (uint32_t) fileStructuresSize, &fileSintez[0], (uint32_t) fileSintezSize, &wm);
		uint16_t response = DLLProcedure(&amplitudeSize, &fileStructures[0], &fileStructuresSize, &fileSintez[0], &fileSintezSize, &wm);
		for (int i = 0; i < fileSintezSize; i++) {
			printf("%o\n", fileSintez[i]);
		}
		if (response < 0) {																				// Если с кодом ошибки
			cout << "Ошибка " << response << "\n";														// Информируем
		} else {
			fileSintezSize = response;																	// Величина присваивается  длине результирующего массива
		}
	} else {																							// Если DLL не найдена
		cout << "Нет хандла библиотеки " << DLLName << "\n";											// Оповещаем
		return  1;																						// Выходим
	}
	FreeLibrary(DLLHandle);																				// Высвобождаем библиотеку

	if (debugInfo) {
		map<int, int> valuesAmplitudes, valuesStraights, valuesObliques;
		for (int i = 0; i < fileStructuresSize; i = i + 4) {
			valuesAmplitudes[fileStructures[i]]++;
			valuesStraights[fileStructures[i + 2]]++;
			valuesObliques[fileStructures[i + 3]]++;
		}

		ofstream file(fileDebug);
		locale mylocale("");
		file.imbue(mylocale);
		auto countQuantity = [&](map<int, int> ma) {
			int sum = 0;
			for (const auto& pair : ma)
				sum += pair.second; // Добавление значения из текущего элемента
			return sum;
		};

		auto countQuality = [&](map<int, int> ma) {
			int sum = 0;
			for (const auto& pair : ma)
				if (pair.first != 0)
					sum += pair.first * pair.second; // Добавление значения из текущего элемента
			return sum;
		};

		auto writeMap = [&](map<int, int> map) {
			for (const auto& pair : map)
				file << pair.first << ": " << pair.second << std::endl;
		};

		file << "Информация по файлу " << *fileStructuresName << "\nКоличество характерных точек " << fileStructuresSize / 4 << "\nЗначения и их количество\n";
		writeMap(valuesAmplitudes);
		file << "\nИнформация по отсчётам по прямой\nвсего " << valuesStraights.size() << ", \nсумма в виде количества " << countQuantity(valuesStraights) << ", \nсумма в виде количества, умноженного на значение " << countQuality(valuesStraights) << "\n";
		writeMap(valuesStraights);
		file << "\nИнформация по отсчётам по косой\nвсего " << valuesObliques.size() << ", \nсумма в виде количества " << countQuantity(valuesObliques) << ", \nсумма в виде количества, умноженного на значение " << countQuality(valuesObliques) << "\n";
		writeMap(valuesObliques);
		file.close();
	}

	ofstream fileOutput(fileOutputName, std::ios::out | std::ios::binary);
	if (fileOutput.is_open()) {
		fileOutput.write(reinterpret_cast<const char*>(fileHeading), fileHeadingSize);
		fileOutput.write(reinterpret_cast<const char*>(fileSintez), fileSintezSize);
		fileOutput.close();
	} else {
		cout << "Ошибка при записи файла" << fileOutputName << "\n";
		return 1;
	}
}

// Запустить питон и передать как параметр два файла
// Ожидать закрытия питона
// PS - py .\Graphs.py t1.dat S1+K1.dat
void getGraphsFromFile(string *fstFile, string *secFile) {
	string command = "cmd /C py Graphs.py ";
	command.append(*fstFile).append(" ").append(*secFile);
	system(command.c_str());
}

/*	Функция для работы с dll фрагментации.
	На вход поступает ссылка на наименование файла или его абсолютный путь, если файл находится не в директории программы.
	Для работы необходима библиотека Fragm. */
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
		length = fileGetSize(file);										// Узнаём размерность файла
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
		} else if (size == (unsigned long)-2) {
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

int createFramesFromWAV(string *fileName) {
	FILE* fileWithHeader = fopen(fileName->c_str(), "rb");				// Считываем файл
	WavHeader header;													// Создаём структуру. куда считываем
	if (fileWithHeader != NULL) {
		fread(&header, sizeof(WavHeader), 1, fileWithHeader);			// Чтение заголовка файла WAV
		fclose(fileWithHeader);											// Закрываем файл
		cout << "Файл успешно прочитан\n";
	}
	else {
		cout << "Ошибка открытия файла\n";
		return 1;
	}
	// Считываем его данные и заголовок

	if (header.blockAlign == 1) vector <uint8_t> toQuant;
	else if (header.blockAlign == 2) vector <uint16_t> toQuant;
	else if (header.blockAlign == 3) vector <uint32_t> toQuant;

	// На основе заголовка делаем разбиение (длина в частоту на время (от 1 до 5)
	int sampleRateMul = 1;
	int frameSize = header.sampleRate * sampleRateMul;
	int size = ceil(header.chunkSize / frameSize);
	string frameName = ".tun";		// Пока реализовываем для 1 канала, для двух необходимо иначе делить

	SF_INFO fileInfo;
	SNDFILE *fileWAV = sf_open(fileName->c_str(), SFM_READ, &fileInfo);
	if (fileWAV == NULL) {
		cout << "Данный файл не существует или недоступен! Программа будет прекращена\n";
		return -1;
	}
	vector <uint8_t> arrayWAV;
	int positionAfterFoundExtremum = 0, iteration = 1;
	readAmplitudesFromWAVTypeUnsignedByte(fileName, fileInfo, arrayWAV);

	if (header.numChannels == 1) {	// Уточням каналы (При 2 каналах необходимо чередование)
		for (int i = 0; i < size; i++) {
			ofstream fileWithAmplitudes(fileName->c_str() + frameName + to_string(i), ios::out);	// не сохраняет изменения
			if (fileWithAmplitudes.is_open()) {
				int tempFrameSize = frameSize;
				for (int j = positionAfterFoundExtremum; j < frameSize * iteration; j++) {
					fileWithAmplitudes << arrayWAV.at(j);
					if (j == frameSize - 1) {
						if (arrayWAV.at(j) > arrayWAV.at(j + 1)) {
							frameSize = frameSize + 1;
						}
						else {
							positionAfterFoundExtremum = positionAfterFoundExtremum + frameSize;
							frameSize = tempFrameSize;
							iteration++;
							break;
						}
					}
				}
			}
			// Придумать деление файла (берем по семпл рейт на множитель
			// Сверяем. что следующая амплитуда будет выше предыдущей. Если да - закончить, иначе ищем.
			fileWithAmplitudes.close();
			// Каждый новый файл сохраняем как .tunN
		}
	}
	// Уточняем размер SampleRate - частота, blockAlign - размер амплитуды и chunkSize - суммарный размер
	return 0;
}

int createSegments(string *filename, int sampleRate, int *delta, bool *debugInfo) {
	string fileOutputName = *filename, fileDebug = "debug_segments_" + *filename + "_.txt";																	// Имя для сегментов
	fileOutputName.replace(filename->length() - 6, 6, "s1.comfra");										// Новое имя для первого сегмента
	int fileComFragSize = 0, fileOutputNumber = 1, countSumComFrag = 0, countBetweenAddresses = 0, addressWithConditionDelta = 0;		// Размер файла, сумма отсчётов, счётчик записанных сегментов, счётчик амплитуд между адресами для сверки с дикретизацией
	uint16_t addressComFrag = 0;																		// Адрес первого указателя
	vector <uint16_t> addressesComFrag;																	// Создание массива с будующими адресами

	cout << "Считывание файла совокупной фрагментации из '" << *filename << "' \n";
	uint8_t* fileComFrag = fileReadRAW(filename, &fileComFragSize);										// -- Считыванием файла --
	while (addressComFrag < fileComFragSize) {															// В цикле, пока не прочитаем весь файл (если файл кончился - указатель будет за пределы файла)
		addressesComFrag.push_back(addressComFrag);														// Пишем первый адрес (по умолчанию 0)
		uint16_t addressComFragPrevious = addressComFrag;												// Записываем предыдущий адрес (для проверки)
		addressComFrag = fileComFrag[addressComFrag + 28] ^ (fileComFrag[addressComFrag + 29] << 8);	// Вырабатываем адресследующего сегмента
		//countSumComFrag = countSumComFrag + (fileComFrag[addressComFrag + 10] ^ (fileComFrag[addressComFrag + 11] << 8)); // Должно быть 21504, но навскидку верно (21442)
		if (addressComFragPrevious > addressComFrag) {													// Если указатель указывает назад, то оповещаем о проблеме
			cout << "Ошибка адресации совокупного фрагмента\n";											// Оповещаем
			return -1;
		}
	}

	queue <bool> condition;																				// Условие, при котором мы говорим, что сенгмент не сегмент
	vector <uint16_t> segmentsAddressesStartAndEnd;														// Вектор с началом и концом сегмента (кратный 2)
	bool flag = false;																					// Флаг, для условия проверки
	for (int i = 0; i < addressesComFrag.size(); i++) {
		addressComFrag = addressesComFrag[i];															// Присваиваем адрес
		uint16_t product = fileComFrag[addressComFrag + 6] - fileComFrag[addressComFrag + 2];			// Находим разницу между экстремумами
		countBetweenAddresses = countBetweenAddresses + (fileComFrag[addressComFrag + 10] ^ (fileComFrag[addressComFrag + 11] << 8));	// счётчик суммы амплитуд между двумя адресами
		countSumComFrag = countSumComFrag + (fileComFrag[addressComFrag + 10] ^ (fileComFrag[addressComFrag + 11] << 8));				// счётчик суммы амплитуд по ВСЕМУ файлу

		if (debugInfo) if (product > *delta) addressWithConditionDelta++;								// Если врублен дебаг - записываем, сколько всего фрагментов удовлетворяет условию

		if (product > *delta) condition.push(true);														// Если больше дельты, то ИС, говорим тру
		else condition.push(false);																		// Если меньше дельты, то МИС, говорим фолс

		if (condition.size() == 3) {																	// Если мы достигли 3 условий, то начинаем смотреть на совокупность
			int conditionDelta = 0;
			for (int i = 0; i < condition.size(); i++) {												// Цикл просмотра на то, кто тру и как много
				if (condition.front() == true) conditionDelta++;										// Счётчик
				condition.push(condition.front());														// вставляем вперёд наше следующее значение
				condition.pop();																		// удаляем его (перебираем стопку тарелок)
			}
			condition.pop();																			// удаляем первое значение (результат в переменной)
			if (conditionDelta > 2) {																	// Если больше 2 (то есть все 3 тру)
				if (flag == false) {																	// И флаг фолс
					countBetweenAddresses = 0;															// Начинаем отсчёт разницы адресов
				}
				if (countBetweenAddresses == 0) {														// Если разница 0
					segmentsAddressesStartAndEnd.push_back(addressComFrag);								// Записываем адрес начала
					flag = true;																		// Флаг переводим в тру
				}
			}
			if (conditionDelta < 1 & countBetweenAddresses > sampleRate) {								// Если условие фолс и разница между больше дискретизации
				segmentsAddressesStartAndEnd.push_back(addressComFrag);									// Записываем адрес
				flag = false;																			// Флаг переводм в фолс
			}
			if (sampleRate * 2 >= countBetweenAddresses) {												// Если мы превысили 2 секунды
				if (sampleRate * 2 == countBetweenAddresses) {											// Мб ровно на 2 секунде?
					segmentsAddressesStartAndEnd.push_back(addressComFrag);								// Записываем адрес
					flag = false;
				}
				if (sampleRate * 2 > countBetweenAddresses) {											// Если превысили, то заберём предыдущий адрес
					segmentsAddressesStartAndEnd.push_back(addressesComFrag[i - 1]);					// Записываем адрес
					flag = false;																		// Флаг переводм в фолс
				}
			}
		}
	}
	segmentsAddressesStartAndEnd.push_back(addressComFrag);												// Записали конечный адрес для сегмента (стоит скорректировать)

	if (*debugInfo) {
		ofstream file(fileDebug);
		locale mylocale("");
		file.imbue(mylocale);

		file << "Информация по файлу " << *filename << "\nРазмер файла " << fileComFragSize
			<< "\nКоличество амплитуд по фрагментам " << countSumComFrag << "\nКоличество адресов, удовлетворяющих дельте " << addressWithConditionDelta
			<< "\nВсего адресов " << addressesComFrag.size();

	}

	uint16_t* arrayComFragAdress = &addressesComFrag[0];
	for (int i = 0; i < segmentsAddressesStartAndEnd.size(); i = i + 2) {
		cout << "Запись файла фрагментов " << fileOutputName << "\n";
		ofstream fileOutput(fileOutputName, std::ios::out | std::ios::binary);
		if (fileOutput.is_open()) {
			int end = segmentsAddressesStartAndEnd[i + 1] - segmentsAddressesStartAndEnd[i] + 1;
			fileOutput.write(reinterpret_cast<const char*>(&arrayComFragAdress[segmentsAddressesStartAndEnd[i]]), end);
			fileOutput.close();
			fileOutputNumber++;
			fileOutputName[fileOutputName.length() - 8] = *to_string(fileOutputNumber).c_str();
		}
		else {
			cout << "Ошибка при записи файла" << fileOutputName << "\n";
			return 1;
		}
	}
	return 1;
}

int main(int argc, char* argv[]) {
	setlocale(LC_ALL, "rus");
	string compare = "/c", compareWAV = "/cwav", amplitudes = "/amp", primitiv = "/p";
	if (argc <= 1) {
		bool mode = false;
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
			"Создать из WAV кадры - 9\n"
			"Создать из ComFrag Сегменты - 10\n"
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
			getFilesWithAmplitudes(&fstFile, &secFile, &channel, &mode);
		} else if (choose == "2") {
			cout << "Введите название первого музыкального файла: ";
			getline(cin, fstFile);
			cout << "Введите название второго музыкального файла: ";
			getline(cin, secFile);
			compareWAVfiles(&fstFile, &secFile);
		} else if (choose == "3") {
			cout << "Введите название музыкального файла типа WAV: ";
			getline(cin, fstFile);
			getFileWithAmplitudesToText(&fstFile, &mode);
		} else if (choose == "4") {
			cout << "Укажите название файла структур: ";
			getline(cin, fstFile);
			cout << "Укажите название файла, содержащий служебную информацию: ";
			getline(cin, secFile);
			sintezWavFromUNIPRIM(&fstFile, &secFile, true);
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
		} else if (choose == "9") { // TODO: переписать код под оптимизацию и сокращение строк, а так же сделать шаблоны
			cout << "Укажите файл, будут созданы кадры: ";
			getline(cin, fstFile);
			createFramesFromWAV(&fstFile);
		} else if (choose == "10") {
			cout << "Укажите файл, из которого будет создан Сегмент: ";
			getline(cin, fstFile);
			cout << "Укажите дельту, по которой будут отсечения: ";
			getline(cin, channels);
			int delta = atoi(channels.c_str());
			mode = true;
			createSegments(&fstFile, 11025, &delta, &mode);
		} else {
			return 0;
		}
		system("pause");
		system("cls");
		goto start;
	} else {
		bool mode = true;
		if (argc > 1 && !compare.compare(argv[1])) {							// для проверки - /c list1.dat list2.dat
			string fstFileToCompare = argv[2];
			string secFileToCompare = argv[3];
			int channels = atoi(argv[4]);
			getFilesWithAmplitudes(&fstFileToCompare, &secFileToCompare, &channels, &mode);
		} else if (argc > 1 && !compareWAV.compare(argv[1])) {					// для проверки - /cwav 11.wav "Coldplay - Viva La Vida (low).wav"
			string fstFileToCompare = argv[2];
			string secFileToCompare = argv[3];
			compareWAVfiles(&fstFileToCompare, &secFileToCompare);
		} else if (argc > 1 && !amplitudes.compare(argv[1])) {					// для проверки - /amp s1.wav
			string fileName = argv[2];
			getFileWithAmplitudesToText(&fileName, &mode);
		} else if (argc > 1 && !primitiv.compare(argv[1])) {					// для проверки - /p s1.txt k1.txt
			string filewithPrimitivs = argv[2];
			string serviceFile = argv[3];
			sintezWavFromUNIPRIM(&filewithPrimitivs, &serviceFile, false);
		}
		return 0;
	}
}

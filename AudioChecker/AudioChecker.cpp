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
#include <limits>
#include <cstdint>

using namespace std;

#pragma pack(push, 1)
struct WavHeader {
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
};
#pragma pack(pop)

int getFileWithAmplitudesToText(string *fileName, bool *mode);
void compareWAVfiles(string *fstFileToCompare, string *secFileToCompare, bool *mode);
void readAmplitudesFromWAVTypeUnsignedByte(string *fileName, SF_INFO fileInfo, vector <uint8_t>& vectorToAmplitudes);
void getFilesWithAmplitudes(string *fileFirstName, string *fileSecondName, int *channels, bool *mode);
void compareRAWData(uint8_t* fileFirstCompare, int fileFirstCompareSize, string fileFirstCompareName, uint8_t *fileSecondCompare, string fileSecondCompareName, int *channels, bool *mode);
void comparatorArrayRAW(uint8_t *arrayFirst, uint8_t *arraySecond, ofstream &fileCompare, int *arraySize, int delta);
streampos fileGetSize(ifstream& file);
uint8_t* fileReadWAVRAW(string *fileName, int *fileSize, WavHeader *wav);
uint8_t* fileReadRAW(string *fileName, int *fileSize);
int synthesizeWavFromUNIPRIM(string& fileStructuresName, const string& fileHeadingName, bool debugInfo);
void getGraphsFromFile(string *fstFile, string *secFile);
int createFragments(string *fileName);
int sintezFragments(string *fileName);
bool getWavHeaderFromFile(const string& fileName);
int createFramesFromWAV(string *fileName);
int createSegmentsA(string *filename, int sampleRate, int *delta, bool *debugInfo, bool *mode);
int createSegmentsB(string *filename, int sampleRate, int *delta, bool *debugInfo, bool *mode);

using Byte = uint8_t;
using ByteVector = vector<Byte>;
using DLLFunction = int(__cdecl*)(int32_t amplitudeSize, Byte* fileWithStructures, int* fileWithStructuresSize, Byte* arrayForResult, int* arrayForResultSize);

string changeFileName(const string &fileName, const string &extension, bool typeOfChange) {
	string result = fileName;
	size_t dotPosition = result.find_last_of('.');
	if (typeOfChange) {	// true - меняем окончание, false - дописываем перед началом
		if (dotPosition == string::npos) result += '.' + extension;				// Если точка не найдена, добавляем новое расширение
		else result = result.substr(0, dotPosition + 1) + extension;				// Заменяем расширение
	}
	else {
		if (dotPosition == string::npos) result += extension;					// Если точка не найдена, просто добавляем строку в конец
		else result.insert(dotPosition, extension);									// Вставляем строку перед последней точкой
	}
	return result;
}


int getFileWithAmplitudesToText(string *fileName, bool *mode) {
	string fileResultName;
	int fileSize = 0;
	WavHeader wav{};
	if (mode) fileResultName = changeFileName(changeFileName(*fileName, "_raw", false), "txt", true);
	else fileResultName = "wavtxt.txt";
	UINT8* arrayWAVRAWAmplitudes = (UINT8 *)fileReadWAVRAW(fileName, &fileSize, &wav);
	if (arrayWAVRAWAmplitudes == nullptr) {
		cout << "Не удалось прочитать амплитуды\n";
		return -1;
	}

	ofstream fileResult(fileResultName);
	if (fileResult.is_open()) {
		for (int i = 0; i < wav.subchunk2Size; ++i) {
			//for (int i = 0; i < fileSize - 44; ++i) {
			fileResult << static_cast<int>(arrayWAVRAWAmplitudes[i]) << "\n";
		}
		cout << "Данные записаны в файл " << fileResultName << "\n";
		fileResult.close();
	}
	else {
		cout << "Не удалось создать файл\n";
		delete[] arrayWAVRAWAmplitudes;
		return -1;
	}
	delete[] arrayWAVRAWAmplitudes;
	return 0;
}


/* Функция, отвечающая за сравнение двух wav файлов.
На вход поступает две строки, которые являются названиями файлов. После их получения происходит
их считывание в массив и последующая процедура вызова сравнения.
@fstFileToCompare и @secFileToCompare являются строками с названиями файла.
По итогу работы, в месте запуска программы создаётся файл compareWAV.dat с резульататми сравнения двух файлов. */
void compareWAVfiles(string *fstFileToCompare, string *secFileToCompare, bool *mode) {
	int fileFstSize = 0, fileSecSize = 0, size = 0, channels = 0;
	WavHeader wavFst{}, wavSec{};
	uint8_t* arrayFstWAVRAWAmplitudes;
	uint8_t* arraySecWAVRAWAmplitudes;
	thread fst([&]() {arrayFstWAVRAWAmplitudes = fileReadWAVRAW(fstFileToCompare, &fileFstSize, &wavFst); });
	thread sec([&]() {arraySecWAVRAWAmplitudes = fileReadWAVRAW(secFileToCompare, &fileSecSize, &wavSec); });
	fst.join();
	sec.join();
	if (wavFst.numChannels != wavSec.numChannels) {
		cout << "Количество каналов отличается\n";
		return;
	}
	if (wavFst.chunkSize == wavSec.chunkSize) {
		size = wavFst.chunkSize, channels = wavFst.numChannels;
		compareRAWData(arrayFstWAVRAWAmplitudes, fileFstSize, *fstFileToCompare, arraySecWAVRAWAmplitudes, *secFileToCompare, &channels, mode);
	}
	else if (wavFst.chunkSize != wavSec.chunkSize) {
		if (wavFst.chunkSize > wavSec.chunkSize) size = wavSec.chunkSize, channels = wavSec.numChannels;
		else size = wavFst.chunkSize, channels = wavFst.numChannels;

		cout << "Размерность не равна, будет использован файл с наименьшим количеством амплитуд\n";
		compareRAWData(arraySecWAVRAWAmplitudes, fileSecSize, *secFileToCompare, arrayFstWAVRAWAmplitudes, *fstFileToCompare, &channels, mode);
	}
	cout << "Сравнение файлов прошло успешно!\n";
}

/* Сама функция непосредственного считывания амплитуд из файла. Специально реализована как отдельная функция
для возможности реализации в дальнейшем многопоточности.
На вход поступает @filename - название нашего файла в формате строки, @fileInfo - указатель на структуру данных
данного аудиофайла.
В самой же процедуре считывание файла происходит с помощью библиотеки "libsndfile".
Руководство по данной библиотеке - http://www.mega-nerd.com/libsndfile/api.html */
void readAmplitudesFromWAVTypeUnsignedByte(string *fileName, SF_INFO fileInfo, vector <uint8_t>& vectorToAmplitudes) {
	SNDFILE *fileWAV = sf_open(fileName->c_str(), SFM_READ, &fileInfo);
	if (fileWAV == NULL) {
		cout << "Файл не найден! Работа прекращена";
		return;
	}
	else {
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
			comparatorArrayRAW(fileFirstCompare, fileSecondCompare, fileResultCompare, &fileFirstCompareSize, 1);
		}
		else if (*channels == 2) {
			fileResultCompare << "Сравнение канала 1\n";
			comparatorArrayRAW(fileFirstCompare, fileSecondCompare, fileResultCompare, &fileFirstCompareSize, 1);
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

/* Узнаём длину файла */
streampos fileGetSize(ifstream& file) {
	streampos fsize = 0;
	fsize = file.tellg();
	file.seekg(0, ios::end);
	fsize = file.tellg() - fsize;
	file.seekg(0, ios::beg);
	return fsize;
}

uint8_t* fileReadWAVRAW(string *fileName, int *fileSize, WavHeader *wav) {
	ifstream fileWAVRAW(*fileName, ios::in | ios::binary);
	*fileSize = fileGetSize(fileWAVRAW);
	if (fileWAVRAW.is_open()) {
		fileWAVRAW.read(reinterpret_cast<char*>(wav), sizeof(WavHeader));
		uint8_t* arrayWAWRAWAmplitudes = new uint8_t[wav->chunkSize];
		for (int i = 0; i < wav->chunkSize; i++) fileWAVRAW.read((char*)&arrayWAWRAWAmplitudes[i], 1);
		return arrayWAWRAWAmplitudes;
	}
	else {
		cout << "Ошибка в названии файла " << *fileName << "\n";
		return NULL;
	}
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
int synthesizeWavFromUNIPRIM(string& fileStructuresName, const string& fileHeadingName, bool debugInfo) {
	string fileDebug = changeFileName(fileStructuresName, "_debug_synthesize", false);
	string fileOutputName = debugInfo ? "synthesizedPrimitive.wav" : changeFileName(fileStructuresName, "wav", true);

	LPCWSTR dllName = L"FENick.dll"; //L"FenVod.dll"; 												// Имя DLL
	LPCSTR dllProcedureName = "F@enick";//"F@enVod"; 												// Имя процедуры в DLL

	int fileStructuresSize = 0, fileSynthesizeSize = 0, fileHeadingSize = 0, dataLength = 0;

	cout << "Считывание файла с примитивами из '" << fileStructuresName << "'\n";
	unique_ptr<Byte[]> fileStructures(fileReadRAW(&fileStructuresName, &fileStructuresSize));		// Считываем файл со структурами
	if (!fileStructures) {
		cerr << "Ошибка при чтении файла структур\n";
		return -1;
	}

	int onlyStructuresSize = fileStructuresSize / 4;												// Вычисляем размер результирующего массива
	for (int i = 0; i < fileStructuresSize; i += 4) {
		Byte temporaryNumber = 1 + fileStructures[i + 2] + fileStructures[i + 3]; 					// Амплитуда + по прямой + по косой
		fileSynthesizeSize += temporaryNumber;
	}

	cout << "Считывание заголовка из '" << fileHeadingName << "'\n";								// Считываем файл с заголовком
	WavHeader header;
	{
		ifstream inputFile(fileHeadingName, ios::binary);
		if (!inputFile) {
			cerr << "Ошибка при открытии файла заголовка\n";
			return -1;
		}
		inputFile.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
		if (!inputFile) {
			cerr << "Ошибка при чтении заголовка файла\n";
			return -1;
		}
	}

	HMODULE dllHandle = LoadLibrary(dllName);														// Загрузка DLL и получение адреса функции
	if (!dllHandle) {
		cerr << "Не удалось загрузить библиотеку " << dllName << "\n";
		return 1;
	}

	DLLFunction dllFunction = reinterpret_cast<DLLFunction>(GetProcAddress(dllHandle, dllProcedureName));
	if (!dllFunction) {
		cerr << "Не удалось получить адрес функции " << dllProcedureName << "\n";
		FreeLibrary(dllHandle);
		return 1;
	}

	unique_ptr<Byte[]> fileSynthesize(new Byte[fileSynthesizeSize]{});								// Создаем результирующий массив
	int32_t amplitudeSize = 1; // Задаем величину амплитуды (1, 2, 4)
	int8_t response = dllFunction(amplitudeSize, fileStructures.get(), &onlyStructuresSize, fileSynthesize.get(), &fileSynthesizeSize);	// Вызов функции из DLL
	if (response < 0) {
		cerr << "Ошибка при вызове функции из DLL, код ошибки: " << static_cast<int>(response) << "\n";
		//FreeLibrary(dllHandle);
		//return 1;
	}
	else {
		fileSynthesizeSize = response;
	}
	FreeLibrary(dllHandle);

	for (int i = 0; i < fileSynthesizeSize; ++i) {
		if (fileSynthesize[i] == 0) {
			dataLength = i;
			break;
		}
	}
	if (dataLength == 0) dataLength = fileSynthesizeSize;


	if (debugInfo) {																				// Если включена отладочная информация
		map<int, int> valuesAmplitudes, valuesStraights, valuesObliques;
		for (int i = 0; i < fileStructuresSize; i += 4) {
			valuesAmplitudes[fileStructures[i]]++;
			valuesStraights[fileStructures[i + 2]]++;
			valuesObliques[fileStructures[i + 3]]++;
		}

		ofstream debugFile(fileDebug);
		if (!debugFile) {
			cerr << "Ошибка при создании файла отладки\n";
			return 1;
		}

		auto countQuantity = [](const map<int, int>& m) {
			int sum = 0;
			for (const auto& pair : m) sum += pair.second;
			return sum;
		};

		auto countQuality = [](const map<int, int>& m) {
			int sum = 0;
			for (const auto& pair : m)
				if (pair.first != 0) sum += pair.first * pair.second;
			return sum;
		};

		auto writeMap = [&](const map<int, int>& m) {
			for (const auto& pair : m) debugFile << pair.first << ": " << pair.second << '\n';
		};

		debugFile << "Информация по файлу " << fileStructuresName
			<< "\nКоличество характерных точек: " << onlyStructuresSize
			<< "\nЗначения и их количество:\n";
		writeMap(valuesAmplitudes);

		debugFile << "\nИнформация по отсчётам по прямой\nВсего: " << valuesStraights.size()
			<< "\nСумма количества: " << countQuantity(valuesStraights)
			<< "\nСумма произведений значения на количество: " << countQuality(valuesStraights) << '\n';
		writeMap(valuesStraights);

		debugFile << "\nИнформация по отсчётам по косой\nВсего: " << valuesObliques.size()
			<< "\nСумма количества: " << countQuantity(valuesObliques)
			<< "\nСумма произведений значения на количество: " << countQuality(valuesObliques) << '\n';
		writeMap(valuesObliques);
	}

	ofstream outputFile(fileOutputName, ios::binary);												// Запись результата в файл
	if (!outputFile) {
		cerr << "Ошибка при открытии файла для записи: " << fileOutputName << "\n";
		return 1;
	}

	header.subchunk2Size = dataLength;														// Обновляем размер данных в заголовке
	header.chunkSize = 44 + header.subchunk2Size;
	header.subchunk2Id[0] = 'd';
	header.subchunk2Id[1] = 'a';
	header.subchunk2Id[2] = 't';
	header.subchunk2Id[3] = 'a';

	outputFile.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
	outputFile.write(reinterpret_cast<const char*>(fileSynthesize.get()), dataLength);
	if (!outputFile) {
		cerr << "Ошибка при записи в файл: " << fileOutputName << "\n";
		return 1;
	}

	cout << "Файл '" << fileOutputName << "' успешно записан\n";
	return 0;
}

/*	Запустить питон и передать как параметр два файла
	Ожидать закрытия питона
	PS - py .\Graphs.py t1.dat S1+K1.dat */
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
	}
	else {
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
	}
	else {
		cout << "Нет хандла FRAGM.dll\n";
		return 1;
	}
	FreeLibrary(handleFragm);											// Освобождаем DLL от хандла
	free(arraySfile);													// Высвобождаем память от массива S файла

	FILE* fragFile = fopen("frag.txt", "wb");							// Создаём файл
	if (fragFile != NULL) {												// Если удалось создать
		if (size == (unsigned long)-1) {
			// не помню код ошибки
		}
		else if (size == (unsigned long)-2) {
			cout << "Файл имеет плохую структуру (0 по прямой, 0 по косой)\n";
		}
		else {
			length = size;												// Устанавливаем размер массива, так как он пришёл без ошибок
		}
		fwrite(arrayFfile, sizeof(int8_t), length, fragFile);			// Записываем массив с размером Int_8t и длиной в файл
		fclose(fragFile);												// Закрываем файл
	}
	else {
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
		int *arrayFSize,		// Размер массива S 
		uint8_t *arrayForSFile,			// Указатель на F массив, в который будет записан итоговый массив
		int *arraySSize);		// Размер массива (должен быть в два раза больше S)
	ifstream fileFrags(*fileName, ios::binary);							// Открываем считываение бинарного файла
	int length = 0;													// Длина понадобится в дальнейшем
	char* arrayFfile;													// Делаем указатель на массив с S файлом
	if (fileFrags.is_open()) {											// Если файл открыт
		length = fileGetSize(fileFrags);									// Узнаём размерность файла
		arrayFfile = (char*)malloc(length * sizeof(uint8_t));			// Создаём массив под данные
		fileFrags.read(arrayFfile, length);								// считываем в массив с динамической памятью
	}
	else {
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
		size = defragmentDll(2, &arrayFfile[0], &length, arraySfile, &length);
	}
	else {
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

/* Функция для получения заголовка файла
На вход поступает наименование wav файла, который должен быть открыт и из которого будет извелён заголовок. */
bool getWavHeaderFromFile(const string& fileName) {
	ifstream inputFile(fileName, ios::binary);										// Открываем WAV-файл для чтения в бинарном режиме
	if (!inputFile) {
		cerr << "Ошибка открытия файла: " << fileName << "\n";
		return false;
	}

	WavHeader header;																// Читаем заголовок WAV-файла
	inputFile.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
	if (!inputFile) {
		cerr << "Ошибка чтения заголовка файла\n";
		return false;
	}
	inputFile.close();
	cout << "Файл успешно прочитан\n";

	ofstream outputFile(changeFileName(fileName, "h_wav", true), ios::binary);
	if (!outputFile) {
		cerr << "Не удалось открыть файл для записи\n";
		return false;
	}

	outputFile.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));	// Записываем заголовок в новый файл
	if (!outputFile) {
		cerr << "Ошибка при записи заголовка в файл\n";
		return false;
	}
	outputFile.close();
	cout << "Файл успешно записан\n";
	return true;
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

// Создание из совокупного фрагмента (начало с начала файла) 
int createSegmentsA(string *filename, int sampleRate, int *delta, bool *debugInfo, bool *mode) {
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
		countSumComFrag = countSumComFrag + (fileComFrag[addressComFrag + 10] ^ (fileComFrag[addressComFrag + 11] << 8));				// счётчик суммы амплитуд по ВСЕМУ файлу // Должно быть 21504, но навскидку верно (21442)

		if (debugInfo) if (product > *delta) addressWithConditionDelta++;								// Если врублен дебаг - записываем, сколько всего фрагментов удовлетворяет условию

		if (product > *delta) condition.push(true);														// Если больше дельты, то ИС, говорим тру
		else condition.push(false);																		// Если меньше дельты, то МИС, говорим фолс

		if (condition.size() == 3) {																	// Если мы достигли 3 условий, то начинаем смотреть на совокупность
			byte conditionDelta = 0;
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
			if (conditionDelta <= 1 & countBetweenAddresses > sampleRate) {								// Если условие фолс и разница между больше дискретизации
				segmentsAddressesStartAndEnd.push_back(addressComFrag);									// Записываем адрес
				flag = false;																			// Флаг переводм в фолс
				countBetweenAddresses = 0;
			}
			if (sampleRate * 2 >= countBetweenAddresses) {												// Если мы превысили 2 секунды
				if (sampleRate * 2 == countBetweenAddresses) {											// Мб ровно на 2 секунде?
					segmentsAddressesStartAndEnd.push_back(addressComFrag);								// Записываем адрес
					flag = false;
					countBetweenAddresses = 0;
				}
				if (sampleRate * 2 < countBetweenAddresses) {											// Если превысили, то заберём предыдущий адрес
					segmentsAddressesStartAndEnd.push_back(addressesComFrag[i - 1]);					// Записываем адрес
					flag = false;																		// Флаг переводм в фолс
					countBetweenAddresses = 0;
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
		// возможно стоит дать указатели на адреса 

	}

	for (int i = 0; i < segmentsAddressesStartAndEnd.size(); i = i + 2) {
		cout << "Запись файла фрагментов " << fileOutputName << "\n";
		ofstream fileOutput(fileOutputName, ios::out | ios::binary);
		if (fileOutput.is_open()) {
			int end = segmentsAddressesStartAndEnd[i + 1] - segmentsAddressesStartAndEnd[i];
			fileOutput.write(reinterpret_cast<const char*>(&fileComFrag[segmentsAddressesStartAndEnd[i]]), end);
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

// Создание из совокупного фрагмента (начало с центра) 
int createSegmentsB(string *filename, int sampleRate, int *delta, bool *debugInfo, bool *mode) {
	string fileOutputName = *filename, fileDebug = "debug_segments_" + *filename + "_.txt";																	// Имя для сегментов
	fileOutputName.replace(filename->length() - 6, 6, "s1.comfra");										// Новое имя для первого сегмента
	int fileComFragSize = 0, fileOutputNumber = 1, countSumComFrag = 0, countBetweenAddresses = 0, addressWithConditionDelta = 0;		// Размер файла, сумма отсчётов, счётчик записанных сегментов, счётчик амплитуд между адресами для сверки с дикретизацией
	uint16_t addressComFrag = 0;																		// Адрес первого указателя
	vector <uint16_t> addressesComFrag;																	// Создание массива с будующими адресами
	map <uint16_t, uint16_t> mapComFrag;

	cout << "Считывание файла совокупной фрагментации из '" << *filename << "' \n";
	uint8_t* fileComFrag = fileReadRAW(filename, &fileComFragSize);										// -- Считыванием файла --
	while (addressComFrag < fileComFragSize) {															// В цикле, пока не прочитаем весь файл (если файл кончился - указатель будет за пределы файла)
		addressesComFrag.push_back(addressComFrag);														// Пишем первый адрес (по умолчанию 0)
		uint16_t addressComFragPrevious = addressComFrag;												// Записываем предыдущий адрес (для проверки)
		countSumComFrag = countSumComFrag + (fileComFrag[addressComFrag + 10] ^ (fileComFrag[addressComFrag + 11] << 8)); // счётчик суммы амплитуд по ВСЕМУ файлу // Должно быть 21504, но навскидку верно (21442)
		addressComFrag = fileComFrag[addressComFrag + 28] ^ (fileComFrag[addressComFrag + 29] << 8);	// Вырабатываем адресследующего сегмента
		mapComFrag[countSumComFrag] = addressComFrag;
		if (addressComFragPrevious > addressComFrag) {													// Если указатель указывает назад, то оповещаем о проблеме
			cout << "Ошибка адресации совокупного фрагмента\n";											// Оповещаем
			return -1;
		}
	}

	queue <bool> condition;																				// Условие, при котором мы говорим, что сенгмент не сегмент
	vector <uint16_t> segmentsAddressesStartAndEnd;														// Вектор с началом и концом сегмента (кратный 2)
	bool flag = false;																					// Флаг, для условия проверки


	uint16_t fixedAddress = 0, fixedSum = 0, countBack = 0;
	for (auto it = mapComFrag.begin(); it != mapComFrag.end(); ++it) {
		countBack++;
		if (it->first >= sampleRate) {
			fixedAddress = it->second;
			fixedSum = it->first;
			break;
		}
	}
	auto it = mapComFrag.find(fixedSum);
	uint16_t countBackOnly = countBack;
	for (auto rit = map<uint16_t, uint16_t>::reverse_iterator(it); rit != mapComFrag.rend(); ++rit) {
		addressComFrag = addressesComFrag[countBackOnly];													// Присваиваем адрес
		countBackOnly--;
		uint16_t product = fileComFrag[addressComFrag + 6] - fileComFrag[addressComFrag + 2];			// Находим разницу между экстремумами
		countBetweenAddresses = countBetweenAddresses + (fileComFrag[addressComFrag + 10] ^ (fileComFrag[addressComFrag + 11] << 8));	// счётчик суммы амплитуд между двумя адресами
		countSumComFrag = countSumComFrag + (fileComFrag[addressComFrag + 10] ^ (fileComFrag[addressComFrag + 11] << 8));

		if (debugInfo) if (product > *delta) addressWithConditionDelta++;								// Если врублен дебаг - записываем, сколько всего фрагментов удовлетворяет условию

		if (product > *delta) condition.push(true);														// Если больше дельты, то ИС, говорим тру
		else condition.push(false);																		// Если меньше дельты, то МИС, говорим фолс
		if (condition.size() == 3) {																	// Если мы достигли 3 условий, то начинаем смотреть на совокупность
			byte conditionDelta = 0;
			for (int i = 0; i < condition.size(); i++) {												// Цикл просмотра на то, кто тру и как много
				if (condition.front() == true) conditionDelta++;										// Счётчик
				condition.push(condition.front());														// вставляем вперёд наше следующее значение
				condition.pop();																		// удаляем его (перебираем стопку тарелок)
			}
			condition.pop();																			// удаляем первое значение (результат в переменной)
			if (conditionDelta == 0) {																	// Если больше 2 (то есть все 3 тру)
				if (flag == false) {																	// И флаг фолс
					countBetweenAddresses = 0;															// Начинаем отсчёт разницы адресов
				}
				if (countBetweenAddresses == 0) {														// Если разница 0
					segmentsAddressesStartAndEnd.push_back(addressComFrag);								// Записываем адрес начала
					flag = true;																		// Флаг переводим в тру
					break;
				}
			}
			if (rit->second == 0) {																		// Если условие фолс и разница между больше дискретизации
				segmentsAddressesStartAndEnd.push_back(addressComFrag);									// Записываем адрес
				flag = false;																			// Флаг переводм в фолс
				break;
			}
		}
	}

	for (; it != mapComFrag.end(); ++it) {
		addressComFrag = addressesComFrag[countBack];													// Присваиваем адрес
		countBack++;
		uint16_t product = fileComFrag[addressComFrag + 6] - fileComFrag[addressComFrag + 2];			// Находим разницу между экстремумами
		countBetweenAddresses = countBetweenAddresses + (fileComFrag[addressComFrag + 10] ^ (fileComFrag[addressComFrag + 11] << 8));	// счётчик суммы амплитуд между двумя адресами
		countSumComFrag = countSumComFrag + (fileComFrag[addressComFrag + 10] ^ (fileComFrag[addressComFrag + 11] << 8));

		if (debugInfo) if (product > *delta) addressWithConditionDelta++;								// Если врублен дебаг - записываем, сколько всего фрагментов удовлетворяет условию

		if (product > *delta) condition.push(true);														// Если больше дельты, то ИС, говорим тру
		else condition.push(false);																		// Если меньше дельты, то МИС, говорим фолс
		if (condition.size() == 3) {																	// Если мы достигли 3 условий, то начинаем смотреть на совокупность
			byte conditionDelta = 0;
			for (int i = 0; i < condition.size(); i++) {												// Цикл просмотра на то, кто тру и как много
				if (condition.front() == true) conditionDelta++;										// Счётчик
				condition.push(condition.front());														// вставляем вперёд наше следующее значение
				condition.pop();																		// удаляем его (перебираем стопку тарелок)
			}
			condition.pop();																			// удаляем первое значение (результат в переменной)
			if (conditionDelta <= 1) {																	// Если условие фолс и разница между больше дискретизации
				segmentsAddressesStartAndEnd.push_back(addressComFrag);									// Записываем адрес
				flag = false;																			// Флаг переводм в фолс
				countBetweenAddresses = 0;
				break;
			}
			if (mapComFrag.size() == countBack) {
				segmentsAddressesStartAndEnd.push_back(addressComFrag);
				break;
			}
		}
	}
	if (*debugInfo) {
		ofstream file(fileDebug);
		locale mylocale("");
		file.imbue(mylocale);

		file << "Информация по файлу " << *filename << "\nРазмер файла " << fileComFragSize
			<< "\nКоличество амплитуд по фрагментам " << countSumComFrag << "\nКоличество адресов, удовлетворяющих дельте " << addressWithConditionDelta
			<< "\nВсего адресов " << addressesComFrag.size();
	}

	for (int i = 0; i < segmentsAddressesStartAndEnd.size(); i = i + 2) {
		cout << "Запись файла фрагментов " << fileOutputName << "\n";
		ofstream fileOutput(fileOutputName, ios::out | ios::binary);
		if (fileOutput.is_open()) {
			int end = segmentsAddressesStartAndEnd[i + 1] - segmentsAddressesStartAndEnd[i];
			fileOutput.write(reinterpret_cast<const char*>(&fileComFrag[segmentsAddressesStartAndEnd[i]]), end);
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

// Создание из файла калибров (начало с центра) 
int createSegmentsC(string *filename, bool *debugInfo, bool *mode) {
	string fileOutputName = *filename, fileDebug = "debug_segments_" + *filename + "_.txt";																	// Имя для сегментов
	fileOutputName.replace(filename->length() - 6, 6, "s1.drazmK");										// Новое имя для первого сегмента
	int fileComFragSize = 0, fileOutputNumber = 1;
	vector <uint16_t> addressesComFrag;																	// Создание массива с будующими адресами
	map <uint16_t, uint16_t> mapComFrag;

	cout << "Считывание файла калибров из '" << *filename << "' \n";
	uint8_t* fileDrazmK = fileReadRAW(filename, &fileComFragSize);
	if (!fileDrazmK) return -1;

	uint16_t sizeToIteration = (fileComFragSize - 4) / 2;
	uint16_t sizeToIterationTemp = sizeToIteration;
	vector <uint16_t> segmentsAddressesStartAndEnd;
	queue <bool> condition;
	bool flag = true;
	for (int i = sizeToIteration - 6; i > 0; i = i - 16) {
		uint8_t kalibr = fileDrazmK[i + 3];
		if (kalibr > 18) condition.push(true);														// Если больше дельты, то ИС, говорим тру
		else condition.push(false);																		// Если меньше дельты, то МИС, говорим фолс
		if (condition.size() == 3) {																	// Если мы достигли 3 условий, то начинаем смотреть на совокупность
			byte conditionDelta = 0;
			for (int i = 0; i < condition.size(); i++) {												// Цикл просмотра на то, кто тру и как много
				if (condition.front() == true) conditionDelta++;										// Счётчик
				condition.push(condition.front());														// вставляем вперёд наше следующее значение
				condition.pop();																		// удаляем его (перебираем стопку тарелок)
			}
			condition.pop();																			// удаляем первое значение (результат в переменной)
			if (conditionDelta <= 1) {																	// Если условие фолс и разница между больше дискретизации
				segmentsAddressesStartAndEnd.push_back(sizeToIterationTemp);									// Записываем адрес
				flag = false;																			// Флаг переводм в фолс
				break;
			}
			if (i < 0) {
				segmentsAddressesStartAndEnd.push_back(sizeToIterationTemp);
			}
		}
		sizeToIterationTemp = sizeToIterationTemp - 16;
	}
	sizeToIterationTemp = sizeToIteration;
	condition.pop();
	condition.pop();
	for (int i = sizeToIteration; i < fileComFragSize - 4; i = i + 16) {
		uint8_t kalibr = fileDrazmK[i + 3];
		if (kalibr > 18) condition.push(true);														// Если больше дельты, то ИС, говорим тру
		else condition.push(false);																		// Если меньше дельты, то МИС, говорим фолс
		if (condition.size() == 3) {																	// Если мы достигли 3 условий, то начинаем смотреть на совокупность
			byte conditionDelta = 0;
			for (int i = 0; i < condition.size(); i++) {												// Цикл просмотра на то, кто тру и как много
				if (condition.front() == true) conditionDelta++;										// Счётчик
				condition.push(condition.front());														// вставляем вперёд наше следующее значение
				condition.pop();																		// удаляем его (перебираем стопку тарелок)
			}
			condition.pop();																			// удаляем первое значение (результат в переменной)
			if (conditionDelta <= 1) {																	// Если условие фолс и разница между больше дискретизации
				segmentsAddressesStartAndEnd.push_back(sizeToIterationTemp);									// Записываем адрес
				flag = false;																			// Флаг переводм в фолс
				break;
			}
		}
		sizeToIterationTemp = sizeToIterationTemp + 16;
	}
	for (int i = 0; i < segmentsAddressesStartAndEnd.size(); i = i + 2) {
		cout << "Запись файла фрагментов " << fileOutputName << "\n";
		ofstream fileOutput(fileOutputName, ios::out | ios::binary);
		if (fileOutput.is_open()) {
			int end = segmentsAddressesStartAndEnd[i + 1] - segmentsAddressesStartAndEnd[i];
			fileOutput.write(reinterpret_cast<const char*>(&fileDrazmK[segmentsAddressesStartAndEnd[i]]), end);
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

// Смена всех прямых на косые (учитываем, что если есть ступенька, то для неё 1 по прямой)
int changeAllToAverageOblique(string *fileStructuresName) {
	string fileOutputName = changeFileName(changeFileName(*fileStructuresName, "_AO", false), "uni", true);						// Имя файла, в который будет записан результат
	int fileStructuresSize = 0, fileHeadingSize = 0;											// Переменные с длинами тех или иных файлов

	cout << "Считывание файла с ПРИМИТИВами из '" << *fileStructuresName << "' \n";				// -- Считыванием файла со структурами --
	uint8_t* fileStructures = fileReadRAW(fileStructuresName, &fileStructuresSize);				// Процесс считывания
	if (!fileStructures) return -1;
	UINT32 oblique = 0, straigth = 0;
	for (long i = 0; i < fileStructuresSize; i = i + 4) {										// Цикл для высчитывания результирующего массива
		straigth = straigth + (uint8_t)fileStructures[i + 2];
		oblique = oblique + (uint8_t)fileStructures[i + 3];
	}
	UINT32 allTempPoints = straigth + oblique;
	UINT32 averageOblique = allTempPoints / (fileStructuresSize / 4);
	UINT32 remainderOfTheDivision = allTempPoints - fileStructuresSize / 4 * averageOblique;
	for (long i = 0; i < fileStructuresSize; i = i + 4) {										// Цикл для высчитывания результирующего массива
		if (fileStructures[i] == fileStructures[i + 4] && i != fileStructuresSize) {
			fileStructures[i + 2] = (UINT8)averageOblique;										// Если будет по прямой - не нарушаем структуры, делаем полку
			fileStructures[i + 3] = 0;
		}
		else {
			UINT8 tempNumber = averageOblique;
			if (remainderOfTheDivision != 0) {
				tempNumber += 1;																// На случай нецелочисленного деления (сохраняем этот нюанс)
				remainderOfTheDivision--;
			}
			fileStructures[i + 2] = 0;
			fileStructures[i + 3] = tempNumber;													// Записываем по косой усреднённый вариант
		}
	}

	UINT8 removeLast = fileStructures[fileStructuresSize - 1];
	fileStructures[fileStructuresSize - 1] = 0;
	fileStructures[fileStructuresSize - 5] = fileStructures[fileStructuresSize - 5] + removeLast;

	ofstream fileOutput(fileOutputName, ios::out | ios::binary);
	if (fileOutput.is_open()) {
		fileOutput.write(reinterpret_cast<const char*>(fileStructures), fileStructuresSize);
		fileOutput.close();
	}
	else {
		cout << "Ошибка при записи файла" << fileOutputName << "\n";
		return 1;
	}
}

// Сохраняет топологию, корректируя значения в зависимости от тенденции
void topologyCompressor(vector<uint8_t>& data, bool isIncreasingTrend, int currentIndex, int segmentLength, int thresholdValue, int8_t adjustmentValue, vector <Byte> &deltas) {
	if (currentIndex == 0 || currentIndex == 1) {
		deltas.push_back(0);
		return;
	}

	map<int, int> frequencyMap;
	for (int i = 0; i < segmentLength; i++) {
		int dataValue = data[currentIndex - segmentLength + i];
		frequencyMap[dataValue]++;
	}

	vector<int> keysToAdjust;

	// Шаг 1: Идентифицируем ключи больше или меньше thresholdValue
	for (const auto& pair : frequencyMap) {
		if (isIncreasingTrend) {
			if (pair.first > thresholdValue) keysToAdjust.push_back(pair.first);
		}
		else {
			if (pair.first < thresholdValue) keysToAdjust.push_back(pair.first);
		}
	}

	// Шаг 2: Сортируем ключи
	if (isIncreasingTrend) {
		sort(keysToAdjust.begin(), keysToAdjust.end());
	}
	else {
		sort(keysToAdjust.begin(), keysToAdjust.end(), greater<int>());
	}

	// Шаг 3: Создаём отображение старых ключей в новые
	map<int, int> keyMapping;
	int newKey = 0;
	if (isIncreasingTrend) {
		newKey = thresholdValue + 1;
		if (segmentLength == 1) newKey = thresholdValue;
	}
	else {
		newKey = thresholdValue - 1;
		if (segmentLength == 1) newKey = thresholdValue;
	}
	for (int oldKey : keysToAdjust) {
		keyMapping[oldKey] = newKey;
		if (isIncreasingTrend) newKey++;
		else newKey--;
	}

	map<int, int> adjustedFrequencyMap;
	for (const auto& pair : frequencyMap) {
		int key = pair.first;
		int value = pair.second;
		// Если ключ больше или меньше thresholdValue, заменяем его на новый
		if (isIncreasingTrend) {
			if (key > thresholdValue) key = keyMapping[key];
		}
		else {
			if (key < thresholdValue) key = keyMapping[key];
		}
		adjustedFrequencyMap[key] = value;
	}

	// собираем дельты
	for (const auto& pair : keyMapping) {
		if (isIncreasingTrend) deltas.push_back(pair.first - pair.second);
		else deltas.push_back(pair.second - pair.first);
	}

	// Шаг 5: Обновляем значения в векторе data
	for (int i = 0; i < segmentLength; i++) {
		int dataValue = data[currentIndex - segmentLength + i];
		if (keyMapping.find(dataValue) != keyMapping.end()) {
			if (isIncreasingTrend) {
				data[currentIndex - segmentLength + i] = keyMapping[dataValue] + adjustmentValue;
			}
			else {
				data[currentIndex - segmentLength + i] = keyMapping[dataValue] - adjustmentValue;
			}
		}
	}
}

void processData(vector<uint8_t>& data, int8_t adjustmentValue) {
	bool isIncreasingTrend = false; // true - растём, false - уменьшаемся
	int thresholdValue = 128;
	int segmentLength = 0;
	vector <Byte> deltas;

	for (int i = 0; i < data.size(); i++) {
		if (data[i] > thresholdValue) {
			if (isIncreasingTrend == false) {
				topologyCompressor(data, isIncreasingTrend, i, segmentLength, thresholdValue, adjustmentValue, deltas);
				segmentLength = 0;
			}
			isIncreasingTrend = true;
			segmentLength++;
		}
		else if (data[i] < thresholdValue) {
			if (isIncreasingTrend == true) {
				topologyCompressor(data, isIncreasingTrend, i, segmentLength, thresholdValue, adjustmentValue, deltas);
				segmentLength = 0;
			}
			isIncreasingTrend = false;
			segmentLength++;
		}
		else if (data[i] == thresholdValue) {
			if (i < data.size() - 1) {
				if (data[i] != data[i + 1]) {
					topologyCompressor(data, isIncreasingTrend, i, segmentLength, thresholdValue, adjustmentValue, deltas);
					segmentLength = 0;
				}
			}
			segmentLength++;
		}
	}
	// Обработка последнего сегмента
	topologyCompressor(data, isIncreasingTrend, data.size(), segmentLength, thresholdValue, adjustmentValue, deltas);

	ofstream outputFile("deltas", ios::binary);
	outputFile.write(reinterpret_cast<const char*>(deltas.data()), deltas.size() * sizeof(char));
	outputFile.close();
}

int changeAllPointsWithSavedStructure(string* fileName, string compressionFactor) {
	string outputFileName = changeFileName(changeFileName(*fileName, "_cps", false), "uni", true);		// Имя выходного файла
	int fileSize = 0;
	uint8_t* fileData = fileReadRAW(fileName, &fileSize);
	if (!fileData) return -1;

	vector<uint8_t> data;																				// Выбираем каждый 4-й байт из fileData
	for (size_t i = 0; i < fileSize; i += 4) data.push_back(fileData[i]);

	processData(data, static_cast<int8_t>(atoi(compressionFactor.c_str())));							// Обрабатываем данные

	for (size_t i = 0, j = 0; i < fileSize && j < data.size(); i += 4, ++j) fileData[i] = data[j];		// Записываем измененные данные обратно в fileData

	ofstream outputFile(outputFileName, ios::binary);													// Записываем измененные данные в выходной файл
	if (!outputFile.is_open()) {
		cerr << "Ошибка при открытии файла для записи: " << outputFileName << "\n";
		delete[] fileData;
		return 1;
	}
	outputFile.write(reinterpret_cast<const char*>(fileData), fileSize);
	outputFile.close();

	cout << "Обработка данных завершена успешно.\n";
	delete[] fileData;
	return 0;
}

#pragma pack(push, 1)
struct Record {
	uint16_t amplitude;  // 2 байта
	uint8_t straight;    // 1 байт (по прямой)
	uint8_t diagonal;    // 1 байт (по косой)
};
#pragma pack(pop)

int changeAllStraightToOblique(string *inputFilename) {
	string fileOutputName = changeFileName(changeFileName(*inputFilename, "_oblique", false), "uni", true);
	int fileSize = 0;
	uint8_t* rawData = fileReadRAW(inputFilename, &fileSize);
	if (!rawData) return -1;

	size_t numRecords = fileSize / sizeof(Record);

	Record* records = reinterpret_cast<Record*>(rawData);
	vector<Record> outputRecords;
	for (size_t i = 0; i < numRecords; ++i) {
		Record& currRecord = records[i];
		bool removeRecord = false;

		if (currRecord.straight > 0) {
			if (currRecord.diagonal == 0) {
				// Если есть предыдущая запись
				if (i > 0) {
					Record& prevRecord = outputRecords.back();
					prevRecord.diagonal += currRecord.straight + 1;
				}
				else {
					// Если нет предыдущей записи, добавляем к текущему
					currRecord.diagonal = currRecord.straight + 1;
				}
				// Помечаем текущую запись для удаления
				removeRecord = true;
			}
			else {
				// Есть и straight, и diagonal
				currRecord.diagonal += currRecord.straight;
				currRecord.straight = 0;
				// Не удаляем запись
			}
		}

		if (!removeRecord) {
			// Добавляем запись в выходной вектор
			outputRecords.push_back(currRecord);
		}
		// Если запись помечена для удаления, просто не добавляем её в outputRecords
	}


	ofstream fileOutput(fileOutputName, ios::out | ios::binary);
	if (fileOutput.is_open()) {
		fileOutput.write(reinterpret_cast<const char*>(outputRecords.data()), outputRecords.size() * sizeof(Record));
		fileOutput.close();
	}
	else {
		cout << "Ошибка при записи файла " << fileOutputName << "\n";
		delete[] rawData;
		return 1;
	}

	delete[] rawData;
	cout << "Обработка данных завершена успешно.\n";
	return 0;
}

int checkStructureByStraightAndObliqe(string *inputFilename) {
	int fileSize = 0;
	uint8_t* rawData = fileReadRAW(inputFilename, &fileSize);

	// Проверяем, что размер файла кратен размеру структуры Record
	if (fileSize % sizeof(Record) != 0) {
		cerr << "Размер файла не кратен размеру записи.\n";
		delete[] rawData;
		return 1;
	}

	size_t numRecords = fileSize / sizeof(Record);
	Record* records = reinterpret_cast<Record*>(rawData);

	// Итерируем по записям от индекса 0 до предпоследнего
	for (size_t i = 0; i < numRecords - 1; ++i) {
		Record& currRecord = records[i];
		Record& nextRecord = records[i + 1];

		// Проверяем, что байты straight и diagonal ненулевые в обеих записях
		if (currRecord.amplitude == nextRecord.amplitude) {
			if (currRecord.straight == 0 && currRecord.diagonal == 0) {
				if (nextRecord.straight == 0 && nextRecord.diagonal == 0) {
					cout << "Нули по текущей и следующей структуре в направлениях: " << i << "\n";
				}
				cout << "Нули по текущей структуре в направлениях: " << i << "\n";
			}
			if (currRecord.straight > 0 && nextRecord.straight > 0) {
				cout << "Полка по двум структурам сразу" << i << "\n";
			}
		}
		if (currRecord.diagonal > 0 && currRecord.straight > 0) {
			cout << "И полка и тенденция: " << i << "\n";
		}
	}
	delete[] rawData;
	return 0;
}

// Структура для хранения экстремумов
struct Extremum {
	size_t index;
	uint8_t value;
	string type; // "max" или "min"
};

void writeExtremaToFile(const string& fileName, const vector<Record>& records, const vector<Extremum>& extrema, const string& typeFilter = "") {
	ofstream file(fileName);
	if (!file.is_open()) {
		cerr << "Ошибка при открытии файла для записи: " << fileName << "\n";
		return;
	}

	vector<uint8_t> finalData;															// Вектор для хранения конечных данных
	for (size_t i = 0; i < extrema.size(); ++i) {
		if (typeFilter.empty() || extrema[i].type == typeFilter) {
			uint8_t tempStraightAndOblique = 0;
			if (i != 0) {
				size_t startIndex = extrema[i - 1].index + 1;							// Расчет суммарных значений по прямой и по косой между текущим и предыдущим экстремумами
				size_t endIndex = extrema[i].index;

				for (size_t j = startIndex; j < endIndex; ++j) tempStraightAndOblique += records[j].straight + records[j].diagonal;

				// Добавляем данные в finalData
				finalData.push_back(extrema[i].value); // Амплитуда
				finalData.push_back(0); // Зарезервированное значение
				finalData.push_back(records[extrema[i].index].straight);
				finalData.push_back(records[extrema[i].index].diagonal + tempStraightAndOblique);

				if (i >= 8 && finalData[finalData.size() - 8] == finalData[finalData.size() - 4]) {		// Проверка на равенство амплитуд
					finalData[finalData.size() - 6] = finalData[finalData.size() - 5];
					finalData[finalData.size() - 5] = 0;
				}

				if (i >= 12 && finalData[finalData.size() - 9] != 0 && finalData[finalData.size() - 10] != 0) {
					if (finalData[finalData.size() - 12] != 0 && finalData[finalData.size() - 8] != 0) {
						finalData[finalData.size() - 9] = finalData[finalData.size() - 9] + finalData[finalData.size() - 10];
						finalData[finalData.size() - 10] = 0;
					}
					else {
						finalData[finalData.size() - 10] = finalData[finalData.size() - 9] + finalData[finalData.size() - 10];
						finalData[finalData.size() - 9] = 0;
					}
				}
				if (i >= 12 && (finalData[finalData.size() - 4] == finalData[finalData.size() - 8]) && (finalData[finalData.size() - 4] == finalData[finalData.size() - 12])) {
					finalData[finalData.size() - 10] = finalData[finalData.size() - 10] + finalData[finalData.size() - 6];
					finalData[finalData.size() - 6] = 0;
					finalData[finalData.size() - 5] = finalData[finalData.size() - 1];
					finalData.pop_back();
					finalData.pop_back();
					finalData.pop_back();
					finalData.pop_back();
				}
			}
			else {																	// Для первого экстремума просто добавляем его данные
				finalData.push_back(records[extrema[i].index].amplitude);
				finalData.push_back(records[extrema[i].index].straight);
				finalData.push_back(records[extrema[i].index].diagonal);
				finalData.push_back(0);
			}
		}
	}
	if (finalData[finalData.size() - 1] != 0 || finalData[finalData.size() - 2] != 0) {
		finalData.push_back(128);															// Добавляем завершающие данные
		finalData.push_back(0);
		finalData.push_back(0);
		finalData.push_back(0);
	}

	ofstream outputFile(changeFileName(fileName, "uni", true), ios::binary);									// Записываем измененные данные в выходной файл
	if (outputFile.is_open()) {
		outputFile.write(reinterpret_cast<const char*>(finalData.data()), finalData.size());
		outputFile.close();
	}
	else {
		cerr << "Ошибка при записи файла\n";
		return;
	}
	cout << "Обработка данных завершена успешно.\n";

	for (const auto& ext : extrema) 													// Записываем информацию об экстремумах в текстовый файл
		if (typeFilter.empty() || ext.type == typeFilter) file << "Индекс: " << ext.index << ", Значение: " << static_cast<int>(ext.value) << ", Тип: " << ext.type << "\n";
	file.close();
}

// Функция для обработки экстремумов
int processExtremums(string *inputFilename) {
	int fileSize = 0;																	// Чтение данных из файла
	uint8_t* rawData = fileReadRAW(inputFilename, &fileSize);
	if (!rawData) return -1;

	vector<Record> records;																// Преобразуем сырые данные в вектор записей
	for (int i = 0; i + 3 < fileSize; i += 4) {
		Record rec = { rawData[i], rawData[i + 2], rawData[i + 3] };
		records.push_back(rec);
	}
	delete[] rawData;																	// Освобождаем память после использования

	vector<Extremum> extrema;															// Поиск экстремумов, используя алгоритмы непосредственно в коде
	int threshold = 128;
	bool inMax = false, inMin = false;
	uint8_t currentMax = 0, currentMin = 255;
	size_t maxIndex = 0, minIndex = 0;

	for (size_t i = 0; i < records.size(); ++i) {
		uint8_t value = (UINT8)records[i].amplitude;

		if (value > threshold) {														// Обработка максимума
			if (!inMax) {
				inMax = true;
				currentMax = value;
				maxIndex = i;
			}
			else if (value > currentMax) {
				currentMax = value;
				maxIndex = i;
			}
		}
		else if (inMax) {
			inMax = false;
			extrema.push_back({ maxIndex, currentMax, "max" });
		}
		if (value < threshold) {														// Обработка минимума
			if (!inMin) {
				inMin = true;
				currentMin = value;
				minIndex = i;
			}
			else if (value < currentMin) {
				currentMin = value;
				minIndex = i;
			}
		}
		else if (inMin) {
			inMin = false;
			extrema.push_back({ minIndex, currentMin, "min" });
		}
	}

	// Проверка, если максимум или минимум на конце данных
	if (inMax) extrema.push_back({ maxIndex, currentMax, "max" });
	if (inMin) extrema.push_back({ minIndex, currentMin, "min" });


	cout << "Найденные экстремумы:\n";													// Вывод результатов
	for (const auto& ext : extrema) cout << "Индекс: " << ext.index << ", Значение: " << static_cast<int>(ext.value) << ", Тип: " << ext.type << "\n";


	writeExtremaToFile(changeFileName(*inputFilename, "_minima", false), records, extrema, "min");							// Записываем минимумы в файл
	writeExtremaToFile(changeFileName(*inputFilename, "_maxima", false), records, extrema, "max");							// Записываем максимумы в файл
	writeExtremaToFile(changeFileName(*inputFilename, "_extrema", false), records, extrema);								// Записываем все экстремумы в файл
	return 0;
}

int getAllDeltasBetweenZeroAndDot(string *inputFilename) {
	int fileSize = 0;																	// Чтение данных из файла
	UINT8 zero = 128;
	uint8_t* rawData = fileReadRAW(inputFilename, &fileSize);
	if (!rawData) return -1;

	for (int i = 0; i < fileSize; i = i + 4) {
		rawData[i] = zero - rawData[i];
		/*if (rawData[i] > zero) rawData[i] = rawData[i] - zero;
		else if (rawData[i] < zero) rawData[i] = (INT8) abs(zero - rawData[i]);
		else if (rawData[i] == zero) rawData[i] = 0;*/
	}

	ofstream fileOutput(changeFileName(changeFileName(*inputFilename, "_deltas", false), ".uni", true), ios::out | ios::binary);
	if (fileOutput.is_open()) {
		fileOutput.write(reinterpret_cast<const char*>(rawData), fileSize);
		fileOutput.close();
	}
	else {
		cout << "Ошибка при записи файла " << "deltas_" + *inputFilename << "\n";
		delete[] rawData;
		return 1;
	}

	delete[] rawData;
	cout << "Обработка данных завершена успешно.\n";
	return 0;
}

int diagonalProcessing(string *fileName, string *deltaStr) {
	string deltaIn;
	bool type = false;
	int fileStructuresSize = 0;

	cout << "Укажите величину дельты: ";
	getline(cin, deltaIn);
	UINT8 delta = atoi(deltaIn.c_str());
	string fileOutputName = changeFileName(*fileName, "_diagonal" + *deltaStr + deltaIn, false);
	uint8_t* fileStructures = fileReadRAW(fileName, &fileStructuresSize);
	if (!fileStructures) return -1;

	if (*deltaStr == "+") {
		for (long i = 0; i < fileStructuresSize; i = i + 4) {										// Цикл для высчитывания результирующего массива
			if (type) {
				if (fileStructures[i + 2] > 0) fileStructures[i + 2] = fileStructures[i + 2] + delta;
				if (fileStructures[i + 3] > 0) fileStructures[i + 3] = fileStructures[i + 3] + delta;
			}
			else if (fileStructures[i + 3] > 0) {
				fileStructures[i + 3] = fileStructures[i + 3] + delta;
			}
		}
	}
	else if (*deltaStr == "-") {
		for (long i = 0; i < fileStructuresSize; i = i + 4) {										// Цикл для высчитывания результирующего массива
			if (type) {
				if (fileStructures[i + 2] > 0) {
					fileStructures[i + 2] = fileStructures[i + 2] - delta;
					if (fileStructures[i + 2] <= 0 || fileStructures[i + 2] > 128) fileStructures[i + 2] = 1;
				}
				if (fileStructures[i + 3] > 0) {
					fileStructures[i + 3] = fileStructures[i + 3] - delta;
					if (fileStructures[i + 3] <= 0 || fileStructures[i + 3] > 128) fileStructures[i + 3] = 1;
				}
			}
			else if (fileStructures[i + 3] > 0) {
				fileStructures[i + 3] = fileStructures[i + 3] - delta;
				if (fileStructures[i + 3] <= 0 || fileStructures[i + 3] > 128) fileStructures[i + 3] = 1;
			}
		}
	}


	ofstream fileOutput(fileOutputName, ios::out | ios::binary);
	if (fileOutput.is_open()) {
		fileOutput.write(reinterpret_cast<const char*>(fileStructures), fileStructuresSize);
		fileOutput.close();
	}
	else {
		cout << "Ошибка при записи файла" << fileOutputName << "\n";
		return 1;
	}

	return 0;
}


int main(int argc, char* argv[]) {
	setlocale(LC_ALL, "ru-RU");
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);
	string compare = "/c", compareWAV = "/cwav", amplitudes = "/amp", primitiv = "/p", segmentA = "/sA", segmentB = "/sB", segmentC = "/sC";
	if (argc <= 1) {
		bool mode = false, debug = false;
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
			"Создать из ComFrag Сегменты (работа с начала записи) - 10\n"
			"Создать из ComFrag Сегменты (работа с центра записи) - 11\n"
			"Создать из DRAZM   Сегменты (работа с центра записи) - 12\n"
			"Убрать все амплитуды по прямой - 13\n"
			"Сохранить топологию или увеличить характерные точки - 14\n"
			"Только по косой и без полок - 15\n"
			"Проверка структуры на наличие дефектов - 16\n"
			"Поиск экстремумов - 17\n"
			"Высчитать дельты в лоб - 18\n"
			"Работа по диагонали - 19\n"
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
		}
		else if (choose == "2") {
			cout << "Введите название первого музыкального файла: ";
			getline(cin, fstFile);
			cout << "Введите название второго музыкального файла: ";
			getline(cin, secFile);
			compareWAVfiles(&fstFile, &secFile, &mode);
		}
		else if (choose == "3") {
			cout << "Введите название музыкального файла типа WAV: ";
			getline(cin, fstFile);
			getFileWithAmplitudesToText(&fstFile, &mode);
		}
		else if (choose == "4") {
			cout << "Укажите название файла структур: ";
			getline(cin, fstFile);
			cout << "Укажите название файла, содержащий служебную информацию: ";
			getline(cin, secFile);
			synthesizeWavFromUNIPRIM(fstFile, secFile, false);
		}
		else if (choose == "5") {
			cout << "Укажите название певрого файла: ";
			getline(cin, fstFile);
			cout << "Укажите название второго файла: ";
			getline(cin, secFile);
			getGraphsFromFile(&fstFile, &secFile);
		}
		else if (choose == "6") {
			cout << "Укажите название файла, их которого необходимо извлечь заголовок: ";
			getline(cin, fstFile);
			getWavHeaderFromFile(fstFile);
		}
		else if (choose == "7") {
			cout << "Укажите файл, который необходимо фрагментировать: ";
			getline(cin, fstFile);
			createFragments(&fstFile);
		}
		else if (choose == "8") {
			cout << "Укажите файл, из которого необходимо восстановить\nфрагментированную ифнормацию: ";
			getline(cin, fstFile);
			sintezFragments(&fstFile);
		}
		else if (choose == "9") { // TODO: переписать код под оптимизацию и сокращение строк, а так же сделать шаблоны
			cout << "Укажите файл, будут созданы кадры: ";
			getline(cin, fstFile);
			createFramesFromWAV(&fstFile);
		}
		else if (choose == "10") {
			cout << "Укажите файл, из которого будет создан Сегмент: ";
			getline(cin, fstFile);
			cout << "Укажите дельту, по которой будут отсечения: ";
			getline(cin, channels);
			int delta = atoi(channels.c_str());
			createSegmentsA(&fstFile, 11025, &delta, &debug, &mode);
		}
		else if (choose == "11") {
			cout << "Укажите файл, из которого будет создан Сегмент: ";
			getline(cin, fstFile);
			cout << "Укажите дельту, по которой будут отсечения: ";
			getline(cin, channels);
			int delta = atoi(channels.c_str());
			createSegmentsB(&fstFile, 11025, &delta, &debug, &mode);
		}
		else if (choose == "12") {
			cout << "Укажите файл: ";
			getline(cin, fstFile);
			createSegmentsC(&fstFile, &debug, &mode);
		}
		else if (choose == "13") {
			cout << "Укажите файл: ";
			getline(cin, fstFile);
			changeAllToAverageOblique(&fstFile);
		}
		else if (choose == "14") {
			cout << "Укажите файл: ";
			getline(cin, fstFile);
			cout << "Если ввести 0, то будет сохранена топология.\nЗначения выше 0 - прибавление к значениям указанной величины, но до 100.\nВеличина амплитуды: ";
			getline(cin, secFile);
			changeAllPointsWithSavedStructure(&fstFile, secFile);
		}
		else if (choose == "15") {
			cout << "Укажите файл: ";
			getline(cin, fstFile);
			changeAllStraightToOblique(&fstFile);
		}
		else if (choose == "16") {
			cout << "Укажите файл: ";
			getline(cin, fstFile);
			checkStructureByStraightAndObliqe(&fstFile);
		}
		else if (choose == "17") {
			cout << "Укажите файл: ";
			getline(cin, fstFile);
			processExtremums(&fstFile);
		}
		else if (choose == "18") {
			cout << "Укажите файл: ";
			getline(cin, fstFile);
			getAllDeltasBetweenZeroAndDot(&fstFile);
		}
		else if (choose == "19") {
			cout << "Укажите файл: ";
			getline(cin, fstFile);
			cout << "Увеличить или уменьшить? (+ \ -): ";
			getline(cin, secFile);
			diagonalProcessing(&fstFile, &secFile);
		}
		else {
			return 0;
		}
		system("pause");
		system("cls");
		goto start;
	}
	else {
		bool mode = true, debug = false;
		if (argc > 1 && !compare.compare(argv[1])) {							// для проверки - /c list1.dat list2.dat
			string fstFileToCompare = argv[2];
			string secFileToCompare = argv[3];
			int channels = atoi(argv[4]);
			getFilesWithAmplitudes(&fstFileToCompare, &secFileToCompare, &channels, &mode);
		}
		else if (argc > 1 && !compareWAV.compare(argv[1])) {					// для проверки - /cwav 11.wav "Coldplay - Viva La Vida (low).wav"
			string fstFileToCompare = argv[2];
			string secFileToCompare = argv[3];
			compareWAVfiles(&fstFileToCompare, &secFileToCompare, &mode);
		}
		else if (argc > 1 && !amplitudes.compare(argv[1])) {					// для проверки - /amp s1.wav
			string fileName = argv[2];
			getFileWithAmplitudesToText(&fileName, &mode);
		}
		else if (argc > 1 && !primitiv.compare(argv[1])) {					// для проверки - /p s1.txt k1.txt
			string filewithPrimitivs = argv[2];
			string serviceFile = argv[3];
			synthesizeWavFromUNIPRIM(filewithPrimitivs, serviceFile, false);
		}
		else if (argc > 1 && !segmentA.compare(argv[1])) {					// Для проверки - /sA mono8.comfra 11025 10
			string fileName = argv[2];
			int rate = atoi(argv[3]);
			int delta = atoi(argv[4]);
			createSegmentsA(&fileName, rate, &delta, &debug, &mode);
		}
		else if (argc > 1 && !segmentB.compare(argv[1])) {					// Для проверки - /sA mono8.comfra 11025 10
			string fileName = argv[2];
			int rate = atoi(argv[3]);
			int delta = atoi(argv[4]);
			createSegmentsB(&fileName, rate, &delta, &debug, &mode);
		}
		else if (argc > 1 && !segmentC.compare(argv[1])) {					// Для проверки - /sA mono8.comfra 11025 10
			string fileName = argv[2];
			int rate = atoi(argv[3]);
			int delta = atoi(argv[4]);
			createSegmentsC(&fileName, &debug, &mode);
		}
		return 0;
	}
}

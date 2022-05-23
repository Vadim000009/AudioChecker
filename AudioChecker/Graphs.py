import matplotlib.pyplot as plt
import matplotlib as mpl
import numpy as np
import sys
import warnings



mpl.rcParams['lines.linestyle'] = '-'
mpl.rcParams['agg.path.chunksize'] = 100000
fstFileName = sys.argv[1]
secFileName = sys.argv[2]

if __name__ == "__main__":
    warnings.filterwarnings("ignore", category=DeprecationWarning) 
    fstFileName = sys.argv[1]
    secFileName = sys.argv[2]
    #if sys.argv:
    #    exit(1)
    # В genfromtxt указываете файл с амплитудами
    fstFile = np.genfromtxt(fstFileName, delimiter='\n', dtype=np.int)  # np.int_
    secFile = np.genfromtxt(secFileName, delimiter='\n', dtype=np.int)  # np.int_
    plt.plot(fstFile, 'r', label=fstFileName)
    plt.plot(secFile, 'g', label=secFileName)
    plt.xlabel('Количество амплитуд')
    plt.ylabel('Значение амплитуд')
    plt.legend()
    plt.show()

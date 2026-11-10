// Ejercicio 1. Genera una matriz booleana de las dimensiones solicitadas,
// con una probabilidad de 1 elegida por el usuario, la reduce a una matriz
// básica (MB) y calcula su densidad.
// matriz_basica.txt guarda siempre la última MB. Además, cada MB se guarda en
// una copia numerada (matriz_basica_1.txt, matriz_basica_2.txt, ...) que no se
// sobrescribe. El historial guarda ambas matrices y la densidad de cada ejecución.

#include <algorithm> // none_of permite detectar una fila de ceros.
#include <ctime>     // Fecha y hora local de cada registro del historial.
#include <fstream>   // Escritura de archivos con ofstream.
#include <iostream>  // Lectura con cin y mensajes con cout y cerr.
#include <vector>    // Cada matriz es un vector de filas que puede crecer.
#include <random>    // Semilla, generador y distribución aleatoria.
#include <iomanip>   // Formato de la densidad y de la fecha.
#include <sstream>   // Construcción de un registro antes de añadirlo al historial.
#include <string>    // Nombres de los archivos y mensajes de texto.

using namespace std;

//1. Generación de matriz aleatoria de 0s y 1s
// probabilidadUno es la probabilidad de que cada celda sea 1 (0.5 = igual
// probabilidad de 0 y de 1). Permite generar matrices de distinta densidad.
vector<vector<int>> generarMatrizAleatoria(int filas, int columnas, double probabilidadUno)
{
    random_device rd; // Obtiene un valor inicial para la semilla.
    mt19937 gen(rd()); // Inicia el generador pseudoaleatorio Mersenne Twister.
    bernoulli_distribution dis(probabilidadUno); // Devuelve 1 con probabilidad probabilidadUno.

    // Reserva una fila por cada i y una celda por cada columna j.
    vector<vector<int>> matriz(filas, vector<int>(columnas));
    for(int i=0; i<filas; ++i)
    {
        for(int j=0; j<columnas; ++j)
        {
            matriz[i][j]=dis(gen); // Llena la celda (i, j) con el siguiente bit.
        }
    }
    return matriz;
}

// 2. Comprueba si B contiene todos los unos de A y al menos un uno adicional.
// Si B es superfila de A, cubrir A también garantiza cubrir B.
bool esSuperfila(const vector<int>& filaA, const vector<int>&filaB)
{
    bool tieneExtraUnos = false; // Marca si B tiene un 1 donde A tiene un 0.
    
    for(size_t j=0; j<filaA.size(); ++j)
    {
        // Si a B le falta un 1 de A, termina la comprobación de inmediato.
        if(filaA[j]==1 && filaB[j]==0)
        {
            return false;
        }
        if(filaA[j]==0 && filaB[j]==1)
        {
            tieneExtraUnos=true;
        }
    }
    
    return tieneExtraUnos; // Si no hubo un 1 adicional, las filas son iguales.
}

//3. Transformar la matriz original en Matriz Básica (MB)
vector<vector<int>> obtenerMatrizBasica(const vector<vector<int>>& matriz)
{
    vector<vector<int>> sinDuplicados;
    
    // Paso A. De la muestra sintética, conserva una copia de cada fila no nula.
    for(const auto& fila:matriz)
    {
        // Una fila de ceros no puede ser cubierta por ninguna columna.
        // En este ejercicio se descarta como parte de la generación sintética.
        if (none_of(fila.begin(), fila.end(), [](int valor) { return valor == 1; }))
        {
            continue;
        }

        bool estaRepetida = false;
        
        for(const auto& fExistente:sinDuplicados)
        {
            if(fila==fExistente) // La igualdad compara todas las celdas en orden.
            {
                estaRepetida=true;
                break; // Ya se encontró una copia.
            }
        }
        if(!estaRepetida)
        {
            sinDuplicados.push_back(fila); //Agrega filas únicas a la nueva matriz
        }
    }
    // Paso B. Conserva las filas que no contienen propiamente a ninguna otra.
    vector<vector<int>> matrizBasica;
    for(size_t i=0; i<sinDuplicados.size(); ++i)
    {
        bool esSuper=false;
        
        for(size_t j=0; j<sinDuplicados.size(); ++j)
        {
            // Busca una subfila j distinta de i para poder descartar i.
            if(i !=j && esSuperfila(sinDuplicados[j], sinDuplicados[i]))
            {
                esSuper=true; //La fila i es superfila de j -> Se elimina i
                break;
            }
        }
        // Una fila que supera todas las comparaciones es básica.
        if(!esSuper)
        {
            matrizBasica.push_back(sinDuplicados[i]);
        }
    }
    
    return matrizBasica;
}

//4. Calcular la densidad de la Matriz Básica
double calcularDensidad(const vector<vector<int>>& mb)
{
    if(mb.empty() || mb[0].empty()) return 0.0; // si matriz está vacía densidad=0
    
    size_t totalUnos=0;
    size_t filas=mb.size(); //número de filas en Matriz Básica (m)
    size_t columnas=mb[0].size(); //número de columnas en Matriz Básica (n)
    
    //Recorre Matriz Básica y acumula los 1s existentes
    for(size_t i=0; i<filas; ++i)
    {
        for(size_t j=0; j<columnas; ++j)
        {
            if(mb[i][j]==1)
            {
                totalUnos++;
            }
        }
    }
    //Aplicar la fórmula de densidad de una matriz D=Total Unos/(m*n)
    return static_cast<double>(totalUnos) / (static_cast<double>(filas) * columnas);
}

// Escribe cada fila en una línea. Sirve para la consola, el TXT y el historial.
void imprimirMatriz(ostream& salida, const vector<vector<int>>& matriz)
{
    for(const auto& fila:matriz)
    {
        for (size_t j = 0; j < fila.size(); ++j)
        {
            if (j != 0) salida << ' '; // Separa columnas con un espacio.
            salida << fila[j];
        }
        salida << '\n';
    }
}

// Añade un registro por ejecución al documento Markdown, conservando los anteriores.
bool guardarHistorial(const string& nombreArchivo,
                      const vector<vector<int>>& matrizOriginal,
                      const vector<vector<int>>& mb, double densidad,
                      double probabilidadUno, const string& archivoCopia)
{
    // Prepara el registro completo antes de abrir el documento.
    ostringstream registro;
    const time_t ahora = time(nullptr);
    const tm* fechaLocal = localtime(&ahora);
    registro << "\n## Generación de matrices";
    if (fechaLocal)
    {
        registro << " | " << put_time(fechaLocal, "%Y-%m-%d %H:%M:%S");
    }
    registro << "\n\nArchivo: `" << archivoCopia << "` | Probabilidad de 1: "
             << fixed << setprecision(2) << probabilidadUno;

    // Las cercas Markdown mantienen la alineación de las filas al abrir el documento.
    registro << "\n\n### Matriz original (" << matrizOriginal.size() << " x "
             << matrizOriginal.front().size() << ")\n\n```text\n";
    imprimirMatriz(registro, matrizOriginal);
    registro << "```\n\n### Matriz básica (" << mb.size() << " x "
             << mb.front().size() << ")\n\n```text\n";
    imprimirMatriz(registro, mb);
    registro << "```\n\nDensidad de la matriz básica: " << fixed << setprecision(4)
             << densidad << " (" << densidad * 100 << " %)\n";

    // ios::app crea el archivo si hace falta y escribe al final si ya existe.
    ofstream historial(nombreArchivo, ios::app);
    if (!historial)
    {
        return false;
    }
    historial << registro.str();
    historial.close(); // También permite detectar errores al completar la escritura.
    return static_cast<bool>(historial);
}

// Escribe la MB en el formato que leen los Ejercicios 2 y 3:
// primera línea "filas columnas" y luego una fila de bits por línea.
bool guardarMatrizTxt(const string& nombreArchivo, const vector<vector<int>>& mb)
{
    ofstream archivo(nombreArchivo); // Reemplaza el contenido si ya existe.
    if (!archivo)
    {
        return false;
    }
    archivo << mb.size() << ' ' << mb.front().size() << '\n';
    imprimirMatriz(archivo, mb);
    archivo.close();
    return static_cast<bool>(archivo);
}

// Devuelve el primer nombre matriz_basica_N.txt (N = 1, 2, 3, ...) que todavía
// no existe en la carpeta, para no sobrescribir matrices generadas antes.
string siguienteNombreLibre()
{
    for (int numero = 1; ; ++numero)
    {
        const string nombre = "matriz_basica_" + to_string(numero) + ".txt";
        ifstream existente(nombre);
        if (!existente)
        {
            return nombre;
        }
    }
}

// Lee las dimensiones, genera la muestra, presenta el resultado y guarda los archivos.
int main(){
    
    int filas{}, columnas{};
    
    cout << string(50,'=') << endl;
    cout << "GENERADOR Y REDUCTOR A MATRIZ BÁSICA (EJ.1)" << endl;
    cout << string(50,'=') << endl;
    cout << endl;
    // Valida que ambas dimensiones sean enteros positivos.
    cout << "Ingrese el numero de filas (ej.100): ";
    if (!(cin >> filas) || filas <= 0)
    {
        cerr << "El numero de filas debe ser un entero positivo.\n";
        return 1;
    }
    cout << "Ingrese el numero de columnas (ej.10): ";
    if (!(cin >> columnas) || columnas <= 0)
    {
        cerr << "El numero de columnas debe ser un entero positivo.\n";
        return 1;
    }
    // Probabilidad de que cada celda sea 1. Controla la densidad de la matriz
    // generada (la matriz básica suele quedar con una densidad algo menor).
    double probabilidadUno{};
    cout << "Ingrese la probabilidad de que cada celda sea 1 (entre 0 y 1, ej. 0.5): ";
    if (!(cin >> probabilidadUno) || probabilidadUno <= 0.0 || probabilidadUno > 1.0)
    {
        cerr << "La probabilidad debe ser un numero mayor que 0 y menor o igual que 1.\n";
        return 1;
    }

    // Conserva la misma muestra que se usa para obtener la matriz básica.
    // Si toda la muestra es nula, vuelve a generar antes de guardar el resultado.
    vector<vector<int>> matrizOriginal;
    vector<vector<int>> mb;
    do
    {
        matrizOriginal = generarMatrizAleatoria(filas, columnas, probabilidadUno);
        mb = obtenerMatrizBasica(matrizOriginal);
    }
    while (mb.empty());
    cout << endl;
    cout << "Matriz aleatoria generada con exito: (" << filas << " x " << columnas << ")" << endl;
    cout << endl;
    
    // Calcula la proporción de unos de la matriz reducida.
    double densidad = calcularDensidad(mb);
    
    // Muestra las dimensiones antes y después de reducir, y la densidad de MB.
    cout << string(50,'=') << endl;
    cout << "RESULTADOS DE LA REDUCCIÓN A MATRIZ BASICA" << endl;
    cout << string(50,'=') << endl;
    cout << endl;
    cout << "Filas de la Matriz Original: " << filas << endl;
    cout << "Filas de la Matriz Basica: " << mb.size() << endl;
    cout << "Columnas: " << columnas << endl;
    cout << fixed << setprecision(2);
    cout << "Probabilidad de 1 usada al generar: " << probabilidadUno << endl;
    cout << fixed << setprecision(4);
    cout << "Densidad de la Matriz Basica: " << densidad << " (" << densidad*100 << "%)" << endl;
    
    // La consola muestra las matrices básicas pequeñas. El historial las guarda completas.
    if(mb.size()<=15)
    {
        cout << endl << "Contenido de la Matriz Basica (MB): " << endl;
        imprimirMatriz(cout, mb);
    }
    else
    {
        cout << endl << "(La MB tiene " << mb.size() << " filas, se omite su impresion en consola por espacio)." << endl;
    }
    // El TXT conserva el formato que esperan los Ejercicios 2 y 3: dimensiones y bits.
    // matriz_basica.txt se reemplaza en cada ejecución (siempre es la última MB).
    const string nombreArchivo = "matriz_basica.txt";
    if (!guardarMatrizTxt(nombreArchivo, mb))
    {
        cerr << "No se pudo escribir " << nombreArchivo << ".\n";
        return 1;
    }
    // La copia numerada no se sobrescribe, así se conservan todas las MB generadas.
    const string nombreCopia = siguienteNombreLibre();
    if (!guardarMatrizTxt(nombreCopia, mb))
    {
        cerr << "No se pudo escribir " << nombreCopia << ".\n";
        return 1;
    }
    cout << endl << "Matriz básica guardada en " << nombreArchivo
         << " y en " << nombreCopia << ".\n";

    // Guarda las dos matrices y su densidad en el historial acumulativo.
    const string nombreHistorial = "historial_matrices.md";
    if (!guardarHistorial(nombreHistorial, matrizOriginal, mb, densidad,
                          probabilidadUno, nombreCopia))
    {
        cerr << "No se pudo guardar el registro en " << nombreHistorial << ".\n";
        return 1;
    }
    cout << "Registro añadido a " << nombreHistorial << ".\n";
    return 0;
}



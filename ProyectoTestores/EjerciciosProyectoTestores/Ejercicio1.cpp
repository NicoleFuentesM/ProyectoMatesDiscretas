//
//  Ejercicio1:
/*
 Desarrolle un programa que genere matrices booleanas aleatorias, permitiendo especificar el n´umero
 de filas y columnas. El programa debe ser capaz de generar, como m´ınimo, matrices de hasta 100
 filas y 10 columnas. Implemente, adem´as, una rutina que transforme cada matriz generada en su
 correspondiente matriz básica y calcule la densidad de esta ´ultima.
 */
//  Por: Nicole Fuentes

/*
 Lógica implementada: el programa realiza 3 tareas principales:
 1. Generación: Crea una matriz aleatoria de 0s y 1s con dimensiones especificadas por el usuario (filas x columnas)
 2. Reducción a matriz básica:
 - Elimina filas duplicadas.
 - Elimina superfilas (por el principio de absorción: filas que contienen todos los unos de otra fila más unos adicionales), conservando únicamente las filas básicas (las más estrictas o con menos unos)
 3. Cálculo de densidad: Aplicar la fórmula: D= total de 1s en MB/Filas de MB x Columnas de MB
 */

//directivas preprocesador
#include <iostream> //Para usar cin (lectura) y cout (impresión en pantalla)
#include <vector> //Para usar la estructura de datos vector (matriz dinámica) - Vector administra la memoria de manera dinámica, permite cambiar el tamaño en tiempo de ejeciución según ingrese el usuario, ofrece push_back.
#include <random> //Para la generación de números aleatorios
#include <iomanip> //Para dar formato a la salida decimal con (fixed, setprecision)
#include <string> //Para utilizar funciones string.

using namespace std;

//1. Generación de matriz aleatoria de 0s y 1s
vector<vector<int>> generarMatrizAleatoria(int filas, int columnas)
{
    //Generador de números pseudoaleatroios de C++ (<random>) -pseudoaleatorios
    
    random_device rd; //1. Generar semilla (Es un número base que se introduce en un generador de números pseudoaleatorios) basada en el hardware del sistema para que los números cambien en cada ejecución
    
    mt19937 gen(rd()); //2. Motor de generación Mersenne Twister - un algoritmo robusto para la generación de números pseudoaleatorios
    
    uniform_int_distribution<int> dis(0,1); //3. Define la distribución: limita los valores generados estrictamente al entero 0 y 1.
    
    vector<vector<int>> matriz(filas, vector<int>(columnas)); //4. Declara e inicializa la variable con las especificaciones del usuario
    
    //5. Bucle for anidado para llenar cada celda de la matriz
    for(int i=0; i<filas; ++i) //Recorre las filas - i
    {
        for(int j=0; j<columnas; ++j) //Recorre las columnas de cada fila - j
        {
            matriz[i][j]=dis(gen); //llena la matriz con 0 o 1s a la celda [i][j]
        }
    }
    return matriz;
}

//2. Comprobar si la fila A es "subfila" de la fila B (es decir si B es superfila de A, por lo que B se debe eliminar)
bool esSuperfila(const vector<int>& filaA, const vector<int>&filaB)
{
    /*
     B es superfila de A si:
     a) En todas las columnas donde A tiene un 1, B también tiene un 1.
     b) B tiene al menos un 1 extra donde A tiene un 0.
     */
    bool tieneExtraUnos = false; //Variable centinela para comprobar si B tiene al menos un 1 más que A
    
    for(size_t j=0; j<filaA.size(); ++j)
    {
        /*
         Si A tiene un 1 pero B tiene un 0 en la misma columna, significa que B no cubre a A. Por tanto, B NO es superfila de A.
         */
        if(filaA[j]==1 && filaB[j]==0)
        {
            return false; //B no cubre a A
        }
        /*
         Si A tiene un 0 y B tiene un 1, B posee un 1 adicional
         */
        if(filaA[j]==0 && filaB[j]==1)
        {
            tieneExtraUnos=true;
        }
        /*
         Si B cubrió todos los unos de A y además tuvo almenos un 1 extra, se confirma que B es superfila de A (y B debe ser eliminada).
         */
    }
    
    return tieneExtraUnos;
}

//3. Transformar la matriz original en Matriz Básica (MB)
vector<vector<int>> obtenerMatrizBasica(const vector<vector<int>>& matriz)
{
    vector<vector<int>> sinDuplicados;
    
    //Paso A: Eliminar filas duplicados
    for(const auto& fila:matriz)
    {
        bool estaRepetida = false;
        
        for(const auto& fExistente:sinDuplicados)
        {
            if(fila==fExistente) //Comparación directa vector a vector que ofrece C++
            {
                estaRepetida=true;
                break; //SI ya existe no hace falta seguir buscando
            }
        }
        if(!estaRepetida)
        {
            sinDuplicados.push_back(fila); //Agrega filas únicas a la nueva matriz
        }
    }
    //Paso B: Eliminar superfilas (retener solo filas básicas)
    vector<vector<int>> matrizBasica;
    for(size_t i=0; i<sinDuplicados.size(); ++i)
    {
        bool esSuper=false;
        
        for(size_t j=0; j<sinDuplicados.size(); ++j)
        {
            // Compara la fila 'i' contra todas las demás filas 'j'
            if(i !=j && esSuperfila(sinDuplicados[j], sinDuplicados[i]))
            {
                esSuper=true; //La fila i es superfila de j -> Se elimina i
                break;
            }
        }
        //Si la fila 'i' nunca fue superfila de nadie, es una FILA BÁSICA y se conserva
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
    
    int totalUnos=0;
    int filas=(int)mb.size(); //número de filas en Matriz Básica (m)
    int columnas=(int)mb[0].size(); //número de columnas en Matriz Básica (n)
    
    //Recorre Matriz Básica y acumula los 1s existentes
    for(int i=0; i<filas; ++i)
    {
        for(int j=0; j<columnas; ++j)
        {
            if(mb[i][j]==1)
            {
                totalUnos++;
            }
        }
    }
    //Aplicar la fórmula de densidad de una matriz D=Total Unos/(m*n)
    return (double)totalUnos / (filas*columnas);
}

//función imprime la matriz
void imprimirMatriz(const vector<vector<int>>& matriz)
{
    for(const auto& fila:matriz)
    {
        for(int val:fila)
        {
            cout << val << " ";
        }
        cout << endl;
    }
}

//Función Main
int main(){
    
    int filas{}, columnas{};
    
    cout << string(50,'=') << endl;
    cout << "GENERADOR Y REDUCTOR A MATRIZ BÁSICA (EJ.1)" << endl;
    cout << string(50,'=') << endl;
    cout << endl;
    //Entradas de usuario
    cout << "Ingrese el numero de filas (ej.100): ";
    cin >> filas;
    cout << "Ingrese el numero de columnas (ej.10): ";
    cin >> columnas;
    
    //1.Generar la matriz
    vector<vector<int>> matrizOriginal = generarMatrizAleatoria(filas, columnas);
    cout << endl;
    cout << "Matriz aleatoria generada con exito: (" << filas << " x " << columnas << ")" << endl;
    cout << endl;
    
    //2.Obtener la Matriz Básica
    vector<vector<int>> mb = obtenerMatrizBasica(matrizOriginal);
    
    //3.Calcular la densidad
    double densidad = calcularDensidad(mb);
    
    //4.Mostrar resultados
    cout << string(50,'=') << endl;
    cout << "RESULTADOS DE LA REDUCCIÓN A MATRIZ BASICA" << endl;
    cout << string(50,'=') << endl;
    cout << endl;
    cout << "Filas de la Matriz Original: " << filas << endl;
    cout << "Filas de la Matriz Basica: " << mb.size() << endl;
    cout << "Columnas: " << columnas << endl;
    cout << fixed << setprecision(4);
    cout << "Densidad de la Matriz Basica: " << densidad << " (" << densidad*100 << "%)" << endl;
    
    //Si la matriz es pequeña, mostramos la MB en pantalla
    if(mb.size()<=15)
    {
        cout << endl << "Contenido de la Matriz Basica (MB): " << endl;
        imprimirMatriz(mb);
    }
    else
    {
        cout << endl << "(La MB tiene " << mb.size() << " filas, se omite su impresion en consola por espacio)." << endl;
    }
    return 0;
}


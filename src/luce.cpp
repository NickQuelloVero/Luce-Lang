

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <set>
#include <stack>
#include <cstdlib>

using namespace std;


enum BlockType { CLASS_BLOCK, FUNCTION_BLOCK, GENERIC_BLOCK };
bool module_gui_active = false;


stack<BlockType> blockStack;
set<string> declared_variables; 

// TODO: FAR FUNZIONARE IMPORTA GUI
string getGuiModuleCode() {
    return R"====(
    // === MOTORE GRAFICO LUCE ===
    #include <iostream>
    #include <string>
    #include <vector>
    #include <thread>
    #include <chrono>
    
    struct Finestra {
        int width, height;
        std::string title;
        bool is_open = true;
        int frame_count = 0;
        Finestra(std::string t, int w, int h) : title(t), width(w), height(h) {
            std::cout << "\n[GUI] >> Finestra: '" << title << "'" << std::endl;
        }
        bool aperta() { if (frame_count >= 100) is_open = false; return is_open; }
        void aggiorna() {
            frame_count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    };
    struct gui {
        static Finestra* crea_finestra(std::string t, int w, int h) { return new Finestra(t, w, h); }
        static void disegna_rettangolo(Finestra* f, int x, int y, int w, int h, std::string c) {
            std::cout << "   [Frame " << f->frame_count << "] RECT " << c << " @ " << x << "," << y << std::endl;
        }
    };
    )====";
}


int getIndentLevel(const string& line) {
    int spaces = 0;
    for (char c : line) { if (c == ' ') spaces++; else break; }
    return spaces / 4;
}

string cleanLine(string line) {
    size_t first = line.find_first_not_of(' ');
    if (string::npos == first) return "";
    size_t last = line.find_last_not_of(' ');
    string clean = line.substr(first, (last - first + 1));
    if (!clean.empty() && clean.back() == ':') clean.pop_back();
    return clean;
}

vector<string> split(const string &s, char delimiter = ' ') {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}

string inferType(string value) {

    size_t first = value.find_first_not_of(' ');
    if (first != string::npos) value = value.substr(first);

    if (value.front() == '"') return "string"; 
    if (value == "vero" || value == "falso" || value == "true" || value == "false") return "bool";
    if (value.find('.') != string::npos) return "double";
    return "int";
}

pair<string, string> parseFunctionTemplate(string argsRaw) {
    if (argsRaw.empty()) return {"", ""};
    stringstream ss(argsRaw);
    string segment;
    string templates = "template<";
    string args = "";
    int count = 0;
    while(getline(ss, segment, ',')) {
        size_t first = segment.find_first_not_of(' ');
        if (first != string::npos) segment = segment.substr(first);
        size_t last = segment.find_last_not_of(' ');
        if (last != string::npos) segment = segment.substr(0, last + 1);
        
        if (count > 0) { templates += ", "; args += ", "; }
        string typeName = "T" + to_string(count);
        templates += "typename " + typeName;
        args += typeName + " " + segment;
        count++;
    }
    templates += ">\n";
    return {templates, args};
}



string transpile(string sourceCode) {
    stringstream globalStream; 
    stringstream mainStream;   
    stringstream* currentStream = &mainStream; 

    stringstream sourceStream(sourceCode);
    string rawLine;
    vector<string> lines;
    
    module_gui_active = false;
    declared_variables.clear();
    while(!blockStack.empty()) blockStack.pop();

    while (getline(sourceStream, rawLine)) lines.push_back(rawLine);

    globalStream << "#include <iostream>\n#include <vector>\n#include <string>\n#include <algorithm>\n#include <map>\n";
    globalStream << "using namespace std;\n\n";

    for (const string& l : lines) {
        if (l.find("importa gui") != string::npos) module_gui_active = true;
    }
    if (module_gui_active) globalStream << getGuiModuleCode() << "\n";

    int currentIndent = 0;
    bool insideDefinition = false; 

    for (string line : lines) {
        if (line.empty()) continue;

        // Gestione Commenti
        string codeOnly = line;
        string commentPart = "";
        bool inStr = false;
        bool hasComment = false;
        for (size_t i = 0; i < line.size(); i++) {
            if (line[i] == '"') inStr = !inStr;
            if (line[i] == '#' && !inStr) {
                codeOnly = line.substr(0, i);
                commentPart = line.substr(i); 
                hasComment = true;
                break;
            }
        }
        
        int newIndent = getIndentLevel(codeOnly);
        string content = cleanLine(codeOnly);
        
        if (content.empty()) {
            if (hasComment) (*currentStream) << string(newIndent * 4, ' ') << "//" << commentPart.substr(1) << "\n";
            continue;
        }


        while (newIndent < currentIndent) {
            currentIndent--;
            if (!blockStack.empty()) {
                BlockType type = blockStack.top();
                blockStack.pop();
                if (type == CLASS_BLOCK) {
                    (*currentStream) << string(currentIndent * 4, ' ') << "};\n";
                    declared_variables.clear(); 
                } else {
                    (*currentStream) << string(currentIndent * 4, ' ') << "}\n";
                    if (type == FUNCTION_BLOCK) declared_variables.clear();
                }
            }
        }
        
        if (newIndent == 0) {
            insideDefinition = false;
            currentStream = &mainStream; 
        }

        vector<string> words = split(content);
        if (words.empty()) continue;
        string cmd = words[0];


        if (newIndent == 0 && (cmd == "classe" || cmd == "funzione")) {
            currentStream = &globalStream;
            insideDefinition = true;
        }

        (*currentStream) << string(newIndent * 4, ' ');
        currentIndent = newIndent;



        if (cmd == "importa") { (*currentStream) << "// Import header\n"; }
        else if (cmd == "app") { /* Skip */ }
        
        else if (cmd == "classe") { 
            declared_variables.clear(); 
            blockStack.push(CLASS_BLOCK);
            (*currentStream) << "struct " << words[1] << " {\n"; 
        }
        
        else if (cmd == "funzione") {
            declared_variables.clear(); 
            blockStack.push(FUNCTION_BLOCK);
            string rest = content.substr(9);
            size_t openP = rest.find('(');
            string funcName = rest.substr(0, openP);
            string argsRaw = rest.substr(openP + 1, rest.find(')') - openP - 1);
            pair<string, string> templInfo = parseFunctionTemplate(argsRaw);
            
             if (!templInfo.first.empty()) {
                 long pos = (long)(*currentStream).tellp();
                 (*currentStream).seekp(pos - (newIndent * 4)); 
                 (*currentStream) << templInfo.first << string(newIndent * 4, ' ');
             }
            (*currentStream) << "auto " << funcName << "(" << templInfo.second << ") {\n";
        }
        
        else if (cmd.find("costruttore(") == 0) { 
             (*currentStream) << "// Init method suggested instead of constructor\n"; 
        }

        else if (cmd == "ritorno") { (*currentStream) << "return " << content.substr(8) << ";\n"; }
        
        else if (cmd == "scrivi") { (*currentStream) << "cout << " << content.substr(7) << " << endl;\n"; }
        
        else if (cmd == "leggi") {
            string varName = words[1];
            if (declared_variables.find(varName) == declared_variables.end()) {
                (*currentStream) << "int " << varName << "; ";
                declared_variables.insert(varName);
            }
            (*currentStream) << "cin >> " << varName << ";\n";
        }
        
        else if (cmd == "aggiungi") { (*currentStream) << words[3] << ".push_back(" << words[1] << ");\n"; }
        
        else if (cmd == "per") { 
            blockStack.push(GENERIC_BLOCK);
            (*currentStream) << "for(auto " << words[1] << " : " << words[3] << ") {\n"; 
        }
        
        else if (cmd == "se") {
            blockStack.push(GENERIC_BLOCK);
            (*currentStream) << "if (" << content.substr(3) << ") {\n";
        }
        
        else if (cmd == "altrimenti") {

             if (words.size() >= 2 && words[1] == "se") {

                 size_t sePos = content.find("se");
                 string cond = content.substr(sePos + 2); 
                 (*currentStream) << "else if (" << cond << ") {\n";
             } else {

                 (*currentStream) << "else {\n";
             }

             blockStack.push(GENERIC_BLOCK);
        }
        
        else if (cmd == "mentre") {
             blockStack.push(GENERIC_BLOCK);
             string cond = content.substr(7);
             (*currentStream) << "while (" << cond << ") {\n";
        }
        

        else if (content.find("=") != string::npos) {
            size_t eqPos = content.find("=");
            string leftSide = content.substr(0, eqPos);

            leftSide.erase(remove(leftSide.begin(), leftSide.end(), ' '), leftSide.end());
            
            string rightSide = content.substr(eqPos + 1);
            size_t firstCharR = rightSide.find_first_not_of(' ');
            bool rightIsArrayLiteral = (firstCharR != string::npos && rightSide[firstCharR] == '[');


            if (leftSide.find('[') != string::npos) {
                 (*currentStream) << content << ";\n";
            }

            else if (rightIsArrayLiteral) {
                replace(rightSide.begin(), rightSide.end(), '[', '{');
                replace(rightSide.begin(), rightSide.end(), ']', '}');
                string type = (rightSide.find('"') != string::npos) ? "vector<string>" : "vector<int>";
                

                bool isGlobalOrFunction = (blockStack.empty() || blockStack.top() != CLASS_BLOCK);

                if (declared_variables.find(leftSide) == declared_variables.end() && isGlobalOrFunction) {
                    (*currentStream) << type << " " << leftSide << " = " << rightSide << ";\n";
                    declared_variables.insert(leftSide);
                } else {
                     // Se membro classe o già dichiarato
                     if (!blockStack.empty() && blockStack.top() == CLASS_BLOCK) 
                        (*currentStream) << type << " " << leftSide << " = " << rightSide << ";\n";
                     else 
                        (*currentStream) << leftSide << " = " << rightSide << ";\n";
                }
            }

            else {
                // FIX: Se c'è un punto (es: r1.id), è un accesso a membro, NON mette auto
                if (leftSide.find('.') != string::npos) {
                     (*currentStream) << content << ";\n";
                }
                else {
                    bool isNew = (declared_variables.find(leftSide) == declared_variables.end());
                    
                    if (!blockStack.empty() && blockStack.top() == CLASS_BLOCK) {

                        string type = inferType(rightSide);
                        (*currentStream) << type << " " << content << ";\n";
                    } else {

                        if (isNew) {
                            (*currentStream) << "auto " << content << ";\n";
                            declared_variables.insert(leftSide);
                        } else {
                            (*currentStream) << content << ";\n";
                        }
                    }
                }
            }
        }

        else {
            if (module_gui_active && (content.find("gui.") == 0)) {
                 string temp = content;
                 replace(temp.begin(), temp.end(), '.', ':');
                 size_t p = temp.find(':'); if(p!=string::npos) temp.replace(p, 1, "::");
                 (*currentStream) << temp << ";\n";
            } else {
                (*currentStream) << content << ";\n";
            }
        }
    }

    while (currentIndent > 0) {
        currentIndent--;
        if (!blockStack.empty()) {
            BlockType type = blockStack.top();
            blockStack.pop();
            if (type == CLASS_BLOCK) (*currentStream) << string(currentIndent * 4, ' ') << "};\n";
            else (*currentStream) << string(currentIndent * 4, ' ') << "}\n";
        }
    }

    stringstream finalCpp;
    finalCpp << globalStream.str();
    finalCpp << "int main() {\n";
    finalCpp << mainStream.str();
    finalCpp << "    return 0;\n}\n";

    return finalCpp.str();
}


int main(int argc, char* argv[]) {

    if (argc < 2) {
        cout << "\n=== LUCE COMPILER v1.0 ===" << endl;
        cout << "Utilizzo: luce <comando> [file]" << endl;
        cout << "\nComandi disponibili:" << endl;
        cout << "  run <file.luce>    : Compila ed esegue il file specificato" << endl;
        cout << "  docs               : Scarica il manuale di programmazione (manuale_luce.txt)" << endl;
        cout << "===========================\n" << endl;
        return 0;
    }

    string cmd = argv[1];


    if (cmd == "docs") {
        ofstream doc("manuale_luce.txt");
        if (!doc) {
            cout << "[Errore] Impossibile scrivere il file manuale_luce.txt" << endl;
            return 1;
        }

        doc << R"(
=========================================
      MANUALE DI PROGRAMMAZIONE LUCE
=========================================

1. STAMPA E INPUT
-----------------
scrivi "Ciao Mondo"      -> Stampa a video
leggi x                  -> Legge un input e lo salva in x

2. VARIABILI
------------
Non serve dichiarare il tipo, Luce lo capisce da solo:
x = 10                   (Numero intero)
y = 3.14                 (Numero decimale)
nome = "Mario"           (Stringa)
attivo = vero            (Booleano: vero/falso)

3. CONDIZIONI (SE - ALTRIMENTI)
-------------------------------
se x > 10:
    scrivi "Maggiore di 10"
altrimenti se x == 10:
    scrivi "Uguale a 10"
altrimenti:
    scrivi "Minore di 10"

4. CICLI (MENTRE / PER)
-----------------------
# Ciclo While
i = 0
mentre i < 5:
    scrivi i
    i = i + 1

# Ciclo For (su vettori)
nomi = ["Anna", "Luca"]
per nome in nomi:
    scrivi nome

5. VETTORI (LISTE DINAMICHE)
----------------------------
numeri = []              -> Crea lista vuota
aggiungi 5 a numeri      -> Aggiunge elemento
numeri[0] = 1            -> Modifica elemento
x = numeri[0]            -> Legge elemento

6. FUNZIONI
-----------
funzione somma(a, b):
    ris = a + b
    ritorno ris

x = somma(5, 3)

7. CLASSI (OOP)
---------------
classe Cane:
    nome = "Fido"
    eta = 0

# Utilizzo
Cane mio_cane
mio_cane.eta = 5
scrivi mio_cane.nome

8. COMMENTI
-----------
# Questo è un commento e viene ignorato

9. MODULO GUI (GRAFICA)
-----------------------
importa gui
win = gui.crea_finestra("Titolo", 800, 600)

mentre win.aperta():
    gui.disegna_rettangolo(win, 10, 10, 100, 100, "ROSSO")
    win.aggiorna()
        )";
        
        doc.close();
        cout << "[Luce] Ho creato il file 'manuale_luce.txt' in questa cartella." << endl;
        cout << "Aprilo per imparare a programmare in Luce!" << endl;
        return 0;
    }


    if (cmd == "run") {
        if (argc < 3) {
            cout << "[Errore] Devi specificare un file: luce run <nomefile.luce>" << endl;
            return 1;
        }

        string file = argv[2];
        ifstream f(file);
        if (!f.is_open()) { 
            cout << "[Errore] File non trovato: " << file << endl; 
            return 1; 
        }
        
        stringstream buffer; buffer << f.rdbuf();
        string source = buffer.str();
        
        if (source.empty()) {
            cout << "[Errore] Il file e' vuoto." << endl;
            return 1;
        }

        string cppCode = transpile(source);
        
        ofstream out(".temp_build.cpp"); 
        if (!out) { cout << "[Errore] Impossibile creare file temporaneo.\n"; return 1; }
        out << cppCode; out.close();

        string build = "g++ .temp_build.cpp -o app_luce";
        #ifndef _WIN32
            build += " -pthread";
        #endif
        
        if (system(build.c_str()) == 0) {
            #ifdef _WIN32
                system("app_luce.exe");
            #else
                system("./app_luce");
            #endif
        } else {
            cout << "[Luce] ERRORE COMPILAZIONE C++.\n";
            cout << "Suggerimento: controlla la sintassi del tuo file .luce\n";
        }
    } else {
        cout << "Comando sconosciuto: '" << cmd << "'" << endl;
        cout << "Digita 'luce' per la lista dei comandi." << endl;
    }
    return 0;
}
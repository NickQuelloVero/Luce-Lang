#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <set>
#include <stack>
#include <cstdlib>
#include <map>
#include <cmath>
#include <ctime>

using namespace std;

enum BlockType { CLASS_BLOCK, FUNCTION_BLOCK, GENERIC_BLOCK };
bool module_gui_active = false;

stack<BlockType> blockStack;
set<string> declared_variables; 

string getGuiModuleCode() {
    return R"====(
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
        bool aperta() { if (frame_count >= 200) is_open = false; return is_open; }
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

string getMathModuleCode() {
    return R"====(
    // Helper per numeri casuali
    int casuale(int min, int max) {
        return min + (std::rand() % (max - min + 1));
    }
    // Helper per stampa veloce
    template<typename T> void stampa_linea(T val) { std::cout << val << std::endl; }
    )====";
}

string trim(const string& str) {
    size_t first = str.find_first_not_of(' ');
    if (string::npos == first) return "";
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
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


string applyReplacements(string line) {

    vector<pair<string, string>> ops = {
        {"radice(", "sqrt("},
        {"potenza(", "pow("},
        {"modulo(", "abs("},
        {"seno(", "sin("},
        {"coseno(", "cos("},
        {"questo.", "this->"},
        {"nuovo ", "new "} 
    };

    for (auto& op : ops) {
        string from = op.first;
        string to = op.second;
        size_t pos = 0;

        while ((pos = line.find(from, pos)) != string::npos) {
            

            int quotes = 0;
            for (size_t i = 0; i < pos; i++) {
                if (line[i] == '"') quotes++;
            }

            if (quotes % 2 == 0) {

                line.replace(pos, from.length(), to);
                pos += to.length(); 
            } else {

                pos++;
            }
        }
    }
    
    return line;
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
        segment = trim(segment); 
        if (segment.empty()) continue;

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
    globalStream << "#include <cmath>\n#include <cstdlib>\n#include <ctime>\n";
    globalStream << "using namespace std;\n\n";


    globalStream << getMathModuleCode() << "\n";
    for (const string& l : lines) {
        if (l.find("importa gui") != string::npos) module_gui_active = true;
    }
    if (module_gui_active) globalStream << getGuiModuleCode() << "\n";

    int currentIndent = 0;
    bool insideDefinition = false; 

    for (string line : lines) {
        if (line.empty()) continue;

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
        content = applyReplacements(content); 

        // Gestione linee vuote o solo commenti
        if (content.empty()) {
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

            if (newIndent == 0 && insideDefinition) {
                insideDefinition = false;
                currentStream = &mainStream;
            }

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

        if (cmd == "importa") { (*currentStream) << "// Import header gestito\n"; }
        else if (cmd == "app") { }
        

        else if (cmd == "classe") { 
            declared_variables.clear(); 
            blockStack.push(CLASS_BLOCK);
            string className = words[1];
            
            if (words.size() >= 4 && words[2] == ":") {
                string parentClass = words[3];
                (*currentStream) << "struct " << className << " : public " << parentClass << " {\n";
            } else {
                (*currentStream) << "struct " << className << " {\n"; 
            }
        }
        

        else if (cmd == "funzione") {
            bool isMethod = (!blockStack.empty() && blockStack.top() == CLASS_BLOCK);
            blockStack.push(FUNCTION_BLOCK);
            
            string rest = content.substr(9);
            size_t openP = rest.find('(');
            string funcName = trim(rest.substr(0, openP)); 
            string argsRaw = rest.substr(openP + 1, rest.find(')') - openP - 1);
            pair<string, string> templInfo = parseFunctionTemplate(argsRaw);
            

            if (!templInfo.first.empty()) {
                 long pos = (long)(*currentStream).tellp();
                 (*currentStream).seekp(pos - (newIndent * 4)); 
                 (*currentStream) << templInfo.first << string(newIndent * 4, ' ');
            }

            string prefix;
            if (isMethod) {
                if (!templInfo.first.empty()) prefix = "void "; 
                else prefix = "virtual void "; 
            } else {
                prefix = "auto "; 
            }
            
            (*currentStream) << prefix << funcName << "(" << templInfo.second << ") {\n";
        }
        
        else if (cmd.find("costruttore(") == 0) { 
             (*currentStream) << "// Usa un metodo init() o assegna valori di default.\n"; 
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
            size_t ls_first = leftSide.find_first_not_of(' ');
            size_t ls_last = leftSide.find_last_not_of(' ');
            if (ls_first != string::npos) leftSide = leftSide.substr(ls_first, (ls_last-ls_first+1));
            
            string rightSide = content.substr(eqPos + 1);
            size_t firstCharR = rightSide.find_first_not_of(' ');
            bool rightIsArrayLiteral = (firstCharR != string::npos && rightSide[firstCharR] == '[');

            if (leftSide.find('[') != string::npos || leftSide.find("->") != string::npos || leftSide.find('.') != string::npos) {
                 (*currentStream) << content << ";\n";
            }
            else if (rightIsArrayLiteral) {
                replace(rightSide.begin(), rightSide.end(), '[', '{');
                replace(rightSide.begin(), rightSide.end(), ']', '}');
                string type = (rightSide.find('"') != string::npos) ? "vector<string>" : "vector<int>";
                bool insideClass = (!blockStack.empty() && blockStack.top() == CLASS_BLOCK);

                if (declared_variables.find(leftSide) == declared_variables.end() && !insideClass) {
                    (*currentStream) << type << " " << leftSide << " = " << rightSide << ";\n";
                    declared_variables.insert(leftSide);
                } else {
                     if (insideClass) 
                        (*currentStream) << type << " " << leftSide << " = " << rightSide << ";\n";
                     else 
                        (*currentStream) << leftSide << " = " << rightSide << ";\n";
                }
            }
            else {
                bool insideClass = (!blockStack.empty() && blockStack.top() == CLASS_BLOCK);
                bool isNew = (declared_variables.find(leftSide) == declared_variables.end());

                if (insideClass) {
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
    finalCpp << "    std::srand(std::time(0));\n"; 
    finalCpp << mainStream.str();
    finalCpp << "    return 0;\n}\n";

    return finalCpp.str();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "\n=== LUCE LANG v1.2.0 ===" << endl;
        cout << "Utilizzo: luce <comando> [file]" << endl;
        cout << "\nComandi disponibili:" << endl;
        cout << "  run <file.luce>    : Compila ed esegue" << endl;
        cout << "  docs               : Scarica il manuale aggiornato" << endl;
        cout << "=================================================\n" << endl;
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
============================================
   MANUALE DI PROGRAMMAZIONE LUCE v1.2.0
============================================

1. MATEMATICA E CASUALITÀ
-------------------------
x = radice(25)          -> sqrt(25) = 5
y = potenza(2, 3)       -> pow(2, 3) = 8
z = casuale(1, 100)     -> Numero random tra 1 e 100
a = modulo(-10)       -> abs(-10) = 10
s = seno(3.14)          -> sin

2. CLASSI E METODI (OOP)
------------------------
In Luce, le funzioni dentro le classi sono metodi.
Usa 'me' per accedere alle variabili della classe.

classe Eroe:
    vita = 100
    funzione saluta():
        scrivi "Sono un eroe con vita: "
        scrivi questo.vita

    funzione prendi_danno(qta):
        questo.vita = questo.vita - qta

3. EREDITARIETÀ
---------------
Puoi creare classi che ereditano da altre usando ':'

classe Animale:
    funzione verso():
        scrivi "..."

classe Cane : Animale:
    funzione verso():
        scrivi "Bau Bau!"

classe Gatto : Animale:
    funzione verso():
        scrivi "Miao!"

4. POLIMORFISMO
---------------
Grazie all'ereditarietà, puoi usare puntatori per il polimorfismo.

# Assegnazione polimorfica (stile C++)
fido = nuovo Cane()
fido->verso()   -> Stampa "Bau Bau!"

5. INPUT/OUTPUT
---------------
scrivi "Ciao"
leggi x

6. CONDIZIONI E CICLI
---------------------
se x > 10:
    scrivi "Grande"
altrimenti:
    scrivi "Piccolo"

per elemento in lista:
    scrivi elemento
        )";
        
        doc.close();
        cout << "[Luce] Manuale aggiornato creato: manuale_luce.txt" << endl;
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

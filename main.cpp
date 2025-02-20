#include <ncurses.h>
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <limits>

using namespace std;

const int WIDTH = 10;
const int HEIGHT = 20;

const vector<vector<wstring>> TETROMINOES = {
    {L"..X...X...X...X.", L"XXXX"},              // I
    {L"..X..XX..X......", L".X...XX...X...."},    // T
    {L".....XX..XX.....", L"XX...."},            // O
    {L"..X..XX...X.....", L".X...XX...X....."},    // S
    {L".X...XX...X.....", L"..X..XX..X....."},     // Z
    {L"X...X...XX......", L".XX..X...X....."},     // J
    {L"..X...X..XX.....", L"XX...X...X....."}      // L
};

int globalMaxScore = 0;

struct Agent {
    vector<double> weights;
    double fitness;
};

double randomWeight() {
    static random_device rd;
    static mt19937 gen(rd());
    uniform_real_distribution<double> dis(-1.0, 1.0);
    return dis(gen);
}

Agent createRandomAgent() {
    Agent agent;
    agent.weights.resize(4);
    for (int i = 0; i < 4; i++) {
        agent.weights[i] = randomWeight();
    }
    agent.fitness = 0;
    return agent;
}

int aggregateHeight(const vector<vector<int>>& board) {
    int total = 0;
    for (int x = 0; x < WIDTH; x++) {
        int colHeight = 0;
        for (int y = 0; y < HEIGHT; y++) {
            if (board[y][x] != 0) {
                colHeight = HEIGHT - y;
                break;
            }
        }
        total += colHeight;
    }
    return total;
}

int countHoles(const vector<vector<int>>& board) {
    int holes = 0;
    for (int x = 0; x < WIDTH; x++) {
        bool blockFound = false;
        for (int y = 0; y < HEIGHT; y++) {
            if (board[y][x] != 0) {
                blockFound = true;
            } else if (blockFound) {
                holes++;
            }
        }
    }
    return holes;
}

int bumpiness(const vector<vector<int>>& board) {
    vector<int> heights(WIDTH, 0);
    for (int x = 0; x < WIDTH; x++) {
        for (int y = 0; y < HEIGHT; y++) {
            if (board[y][x] != 0) {
                heights[x] = HEIGHT - y;
                break;
            }
        }
    }
    int bump = 0;
    for (int x = 0; x < WIDTH - 1; x++) {
        bump += abs(heights[x] - heights[x + 1]);
    }
    return bump;
}

int completeLines(const vector<vector<int>>& board) {
    int lines = 0;
    for (int y = 0; y < HEIGHT; y++) {
        bool full = true;
        for (int x = 0; x < WIDTH; x++) {
            if (board[y][x] == 0) {
                full = false;
                break;
            }
        }
        if (full) lines++;
    }
    return lines;
}

double evaluateBoard(const vector<vector<int>>& board, const Agent& agent) {
    int aggHeight = aggregateHeight(board);
    int holes = countHoles(board);
    int bump = bumpiness(board);
    int lines = completeLines(board);
    return agent.weights[0] * lines - agent.weights[1] * aggHeight - agent.weights[2] * holes - agent.weights[3] * bump;
}

wstring rotatePiece(const wstring& piece) {
    wstring rotated = piece;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            rotated[x * 4 + (3 - y)] = piece[y * 4 + x];
        }
    }
    return rotated;
}

vector<vector<int>> simulateDrop(const vector<vector<int>>& board, const wstring& piece, int pos_x, int pos_y, int color) {
    vector<vector<int>> newBoard = board;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (piece[y * 4 + x] == L'X') {
                int fx = pos_x + x;
                int fy = pos_y + y;
                if (fy >= 0 && fy < HEIGHT && fx >= 0 && fx < WIDTH) {
                    newBoard[fy][fx] = color;
                }
            }
        }
    }
    for (int y = 0; y < HEIGHT; y++) {
        bool full = true;
        for (int x = 0; x < WIDTH; x++) {
            if (newBoard[y][x] == 0) { full = false; break; }
        }
        if (full) {
            newBoard.erase(newBoard.begin() + y);
            newBoard.insert(newBoard.begin(), vector<int>(WIDTH, 0));
        }
    }
    return newBoard;
}

struct Move {
    int rotation;
    int x; 
    double score;
};

Move computeBestMove(const vector<vector<int>>& board, const wstring& piece, int color, const Agent& agent) {
    Move bestMove;
    bestMove.score = -std::numeric_limits<double>::infinity();
    wstring currPiece = piece;
    for (int r = 0; r < 4; r++) {
        for (int x = -4; x < WIDTH; x++) {
            int y = 0;
            while (true) {
                bool coll = false;
                for (int py = 0; py < 4; py++) {
                    for (int px = 0; px < 4; px++) {
                        if (currPiece[py * 4 + px] == L'X') {
                            int fx = x + px;
                            int fy = y + py;
                            if (fx < 0 || fx >= WIDTH || fy >= HEIGHT) { coll = true; break; }
                            if (fy >= 0 && board[fy][fx] != 0) { coll = true; break; }
                        }
                    }
                    if (coll) break;
                }
                if (coll) break;
                y++;
            }
            y--; 
            if (y < 0) continue;
            vector<vector<int>> newBoard = simulateDrop(board, currPiece, x, y, color);
            double eval = evaluateBoard(newBoard, agent);
            if (eval > bestMove.score) {
                bestMove.score = eval;
                bestMove.rotation = r;
                bestMove.x = x;
            }
        }
        currPiece = rotatePiece(currPiece);
    }
    return bestMove;
}

Agent crossover(const Agent& parent1, const Agent& parent2) {
    Agent child;
    child.weights.resize(4);
    for (int i = 0; i < 4; i++){
        child.weights[i] = (parent1.weights[i] + parent2.weights[i]) / 2.0;
        if ((rand() % 100) < 10) {
            child.weights[i] += randomWeight() * 0.1;
        }
    }
    child.fitness = 0;
    return child;
}

class Game {
private:
    WINDOW *game_win;
    vector<vector<int>> field;
    int score;
    int level;
    int linesClearedTotal;
    wstring current_piece;
    int current_color;
    int pos_x, pos_y;
    bool aiMode;
    Agent agent; 
    chrono::steady_clock::time_point last_update;
    int winTop, winLeft;
    bool finished; 

    void gameOver() {
        mvwprintw(game_win, HEIGHT / 2, WIDTH - 8, "GAME OVER");
        wrefresh(game_win);
        if (score > globalMaxScore) {
            globalMaxScore = score;
        }
        agent.fitness = score;
        finished = true;
    }

    bool collision(int offset_x = 0, int offset_y = 0) {
        for (int y = 0; y < 4; y++){
            for (int x = 0; x < 4; x++){
                if (current_piece[y * 4 + x] == L'X'){
                    int new_x = pos_x + x + offset_x;
                    int new_y = pos_y + y + offset_y;
                    if(new_x < 0 || new_x >= WIDTH) return true;
                    if(new_y >= HEIGHT) return true;
                    if(new_y >= 0 && field[new_y][new_x])
                        return true;
                }
            }
        }
        return false;
    }

    void merge_piece() {
        for (int y = 0; y < 4; y++){
            for (int x = 0; x < 4; x++){
                if (current_piece[y * 4 + x] == L'X'){
                    int fy = pos_y + y;
                    int fx = pos_x + x;
                    if (fy >= 0 && fy < HEIGHT && fx >= 0 && fx < WIDTH)
                        field[fy][fx] = current_color;
                }
            }
        }
    }

    void clear_lines() {
        int linesClearedThisTurn = 0;
        for (int y = 0; y < HEIGHT; y++){
            bool full = true;
            for (int x = 0; x < WIDTH; x++){
                if (!field[y][x]){
                    full = false;
                    break;
                }
            }
            if (full){
                field.erase(field.begin() + y);
                field.insert(field.begin(), vector<int>(WIDTH, 0));
                linesClearedThisTurn++;
            }
        }
        if (linesClearedThisTurn > 0){
            linesClearedTotal += linesClearedThisTurn;
            level = linesClearedTotal / 10 + 1;
            int baseScore = 0;
            switch (linesClearedThisTurn) {
                case 1: baseScore = 40; break;
                case 2: baseScore = 100; break;
                case 3: baseScore = 300; break;
                case 4: baseScore = 1200; break;
                default: baseScore = linesClearedThisTurn * 200; break;
            }
            score += baseScore * level;
        }
    }

    void update_window_position() {
        mvwin(game_win, winTop, winLeft);
    }

public:
    Game(bool ai = true, Agent agentParam = createRandomAgent(), int winTop = 5, int winLeft = 10)
        : score(0), level(1), linesClearedTotal(0), pos_x(WIDTH / 2 - 2), pos_y(0),
          aiMode(ai), agent(agentParam), winTop(winTop), winLeft(winLeft), finished(false)
    {
        field.resize(HEIGHT, vector<int>(WIDTH, 0));
        game_win = newwin(HEIGHT + 2, WIDTH * 2 + 2, winTop, winLeft);
        keypad(game_win, TRUE);
        nodelay(game_win, TRUE);
        spawn_piece();
        last_update = chrono::steady_clock::now();
    }
    
    ~Game() {
        if(game_win) {
            delwin(game_win);
        }
    }
    
    void spawn_piece() {
        static mt19937 rng(random_device{}());
        uniform_int_distribution<int> dist(0, TETROMINOES.size() - 1);
        int type = dist(rng);
        current_piece = TETROMINOES[type][0];
        current_color = type + 1;
        pos_x = WIDTH / 2 - 2;
        pos_y = 0;
        if (collision()){
            gameOver();
            return;
        }
    }

    void draw() {
        wclear(game_win);
        box(game_win, 0, 0);
        for (int y = 0; y < HEIGHT; y++){
            for (int x = 0; x < WIDTH; x++){
                if (field[y][x]){
                    wattron(game_win, COLOR_PAIR(field[y][x]));
                    mvwaddstr(game_win, y + 1, x * 2 + 1, "#");
                    wattroff(game_win, COLOR_PAIR(field[y][x]));
                }
            }
        }
        wattron(game_win, COLOR_PAIR(current_color));
        for (int y = 0; y < 4; y++){
            for (int x = 0; x < 4; x++){
                if (current_piece[y * 4 + x] == L'X'){
                    int px = pos_x + x;
                    int py = pos_y + y;
                    if (py >= 0 && py < HEIGHT && px >= 0 && px < WIDTH)
                        mvwaddstr(game_win, py + 1, px * 2 + 1, "#");
                }
            }
        }
        wattroff(game_win, COLOR_PAIR(current_color));
        mvwprintw(game_win, 0, 2, "Score: %d  Level: %d  Max: %d", score, level, globalMaxScore);
        wrefresh(game_win);
    }

    void rotate() {
        wstring new_piece = current_piece;
        for (int y = 0; y < 4; y++){
            for (int x = 0; x < 4; x++){
                new_piece[x * 4 + (3 - y)] = current_piece[y * 4 + x];
            }
        }
        wstring old_piece = current_piece;
        current_piece = new_piece;
        int offset = 1;
        while (collision()){
            pos_x += offset;
            offset = (offset > 0) ? -offset - 1 : -offset + 1;
            if (abs(offset) > 3){
                current_piece = old_piece;
                break;
            }
        }
    }

    void update() {
        if (finished) return;
        if (!collision(0, 1)){
            pos_y++;
        } else {
            merge_piece();
            for (int x = 0; x < WIDTH; x++){
                if (field[0][x] != 0){
                    gameOver();
                    return;
                }
            }
            clear_lines();
            spawn_piece();
        }
        draw();
    }

    void aiMove() {
        if (finished) return;
        Move best = computeBestMove(field, current_piece, current_color, agent);
        for (int i = 0; i < best.rotation; i++){
            rotate();
        }
        if (pos_x < best.x) {
            while (pos_x < best.x && !collision(1, 0)) pos_x++;
        } else if (pos_x > best.x) {
            while (pos_x > best.x && !collision(-1, 0)) pos_x--;
        }
        while (!collision(0, 1)) {
            pos_y++;
        }
    }

    void handle_input(int key) {
        if (!aiMode && !finished) {
            switch(key){
                case KEY_LEFT:
                    if (!collision(-1, 0)) pos_x--;
                    break;
                case KEY_RIGHT:
                    if (!collision(1, 0)) pos_x++;
                    break;
                case KEY_DOWN:
                    if (!collision(0, 1)) pos_y++;
                    break;
                case ' ':
                    rotate();
                    break;
            }
        }
    }

    void tick() {
        if (finished) return;
        int ch = wgetch(game_win);
        if (ch != ERR)
            handle_input(ch);
        if (aiMode)
            aiMove();
        auto now = chrono::steady_clock::now();
        int currentDelay = max(100, 1000 - (level - 1) * 100);
        if (chrono::duration_cast<chrono::milliseconds>(now - last_update).count() > currentDelay) {
            update();
            last_update = now;
        }
    }
    
    bool isFinished() const { return finished; }
    Agent getAgent() const { return agent; }
};

int main() {
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    init_pair(2, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_GREEN, COLOR_BLACK);
    init_pair(5, COLOR_RED, COLOR_BLACK);
    init_pair(6, COLOR_BLUE, COLOR_BLACK);
    init_pair(7, COLOR_WHITE, COLOR_BLACK);

    int gap = 5;
    int winWidth = WIDTH * 2 + 2;
    int top = 5;
    int left1 = 5;
    int left2 = left1 + winWidth + gap;
    int left3 = left2 + winWidth + gap;

    vector<Agent> agents;
    agents.push_back(createRandomAgent());
    agents.push_back(createRandomAgent());
    agents.push_back(createRandomAgent());

    int generation = 1;
    while (true) {
        clear();
        mvprintw(0, 0, "Generation: %d", generation);
        refresh();
        vector<Game*> games;
        games.push_back(new Game(true, agents[0], top, left1));
        games.push_back(new Game(true, agents[1], top, left2));
        games.push_back(new Game(true, agents[2], top, left3));
        
        bool allFinished = false;
        while (!allFinished) {
            allFinished = true;
            for (auto g : games) {
                if (!g->isFinished()) {
                    g->tick();
                    allFinished = false;
                }
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }
        
        vector<Agent> finishedAgents;
        for (auto g : games) {
            finishedAgents.push_back(g->getAgent());
        }
        
        for (auto g : games) {
            delete g;
        }
        
        printf("Generation %d finished. Scores: %d, %d, %d. Global Max: %d\n", generation,
               (int)finishedAgents[0].fitness, (int)finishedAgents[1].fitness, (int)finishedAgents[2].fitness,
               globalMaxScore);
        
        vector<Agent> newAgents(3);
        newAgents[0] = crossover(finishedAgents[0], finishedAgents[1]);
        newAgents[1] = crossover(finishedAgents[1], finishedAgents[2]);
        newAgents[2] = crossover(finishedAgents[2], finishedAgents[0]);
        agents = newAgents;
        generation++;
    }

    endwin();
    return 0;
}

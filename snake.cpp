#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Box.H>       // Include Fl_Box for displaying images
#include <FL/fl_ask.H>       // Include fl_ask.H for message box
#include <FL/Fl_PNG_Image.H> // Include Fl_PNG_Image for PNG images
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
#include <array>
#include <random>
#include <utility>
#include <iostream>
#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#endif
#ifdef __linux__
#include <alsa/asoundlib.h>
#endif
#include <thread>
#include <chrono>

using namespace std;

#define MUSICFILENAME "snake.wav"

const int BOARD_SIZE = 10;
const int SQUARE_SIZE = 57;
const int BORDER_X = 31;
const int BORDER_Y = 31;
const int WINDOW_WIDTH = BOARD_SIZE * SQUARE_SIZE + 200 + 2 * BORDER_X;
const int WINDOW_HEIGHT = BOARD_SIZE * SQUARE_SIZE + 2 * BORDER_Y;
typedef std::pair<std::pair<int, int>, std::pair<int, int>> Coord;

class DiceWidget : public Fl_Widget
{
    static DiceWidget *_instance;
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<> dist;
    std::array<Fl_PNG_Image *, 6> diceFaces;
    Fl_Box *diceBox;
    Fl_PNG_Image *currentFace;

private:
    DiceWidget(int x, int y, int w, int h, const char *label = nullptr)
        : Fl_Widget(x, y, w, h, label), gen(rd()), dist(1, 6), currentFace(nullptr)
    {

        for (int i = 0; i < diceFaces.max_size(); ++i)
        {
            std::string filename = "dice_" + std::to_string(i + 1) + ".png";
            diceFaces[i] = new Fl_PNG_Image(filename.c_str());
        }
        diceBox = new Fl_Box(680, 266, 100, 100, "");
        diceBox->box(FL_FLAT_BOX); // Set the box type to FL_FLAT_BOX for a flat appearance
        diceBox->color(FL_WHITE);  // Set the background color of the box
    }

    void setDiceImage(unsigned int pos)
    {
        currentFace = diceFaces[pos - 1];
    }

public:
    ~DiceWidget()
    {
        delete diceBox;
        for (int i = 0; i < diceFaces.max_size(); ++i, delete diceFaces[i])
            ;
    }

    static DiceWidget *getInstance(int x, int y, int w, int h, const char *label = nullptr)
    {

        if (_instance == nullptr)
        {
            _instance = new DiceWidget(x, y, w, h, label);
        }
        return _instance;
    }

    void draw() override
    {
        //  Draw dice image

        if (currentFace)
        {
            diceBox->image(currentFace);
            diceBox->size(currentFace->w() + 10, currentFace->h() + 10);
        }
    }

    unsigned int roll()
    {
        unsigned int r = dist(gen);

        setDiceImage(r);
        return r;
    }
};

DiceWidget *DiceWidget::_instance = nullptr;

class PlayerWidget : public Fl_Widget
{
    int position;
    Fl_Color color;
    Fl_Box *playerBox;
    int x, y, w, h;

public:
    PlayerWidget(int x, int y, int w, int h, uint col, const char *label = nullptr)
        : Fl_Widget(x, y, w, h, label), color(col), x(0), y(0), w(SQUARE_SIZE - 10), h(SQUARE_SIZE - 10), position(1)
    {
        playerBox = new Fl_Box(0, 0, w, h, "");
        playerBox->color(col);
        playerBox->box(FL_FLAT_BOX);
    }

    ~PlayerWidget()
    {
        delete playerBox;
    }

    void updatePosition(int pos) { position = pos; }

    int getPosition() const { return position; }

    void updateCoordinates(int x, int y, int w, int h)
    {
        this->x = x;
        this->y = y;
        this->w = w;
        this->h = h;
    }

    void draw() override
    {
        playerBox->resize(x, y, w, h);
    }
};

class SoundDriver
{
protected:
    std::string m_filename;
    std::thread *_soundThread;

public:
    SoundDriver(std::string filename) : m_filename(filename), _soundThread(nullptr) {}

    virtual ~SoundDriver()
    {
        {
            if (_soundThread)
                delete _soundThread;
        }
    }

    virtual void playSound() = 0;
};

class SnakeLadderGameSound : public SoundDriver
{

public:
    SnakeLadderGameSound(std::string filename) : SoundDriver(filename) {}

    void playSound() override
    {

#ifdef __APPLE__
        _soundThread = new thread(&SnakeLadderGameSound::playSound_MAC, this);
        _soundThread->detach();
#elif __linux__
        _soundThread = new thread(&SnakeLadderGameSound::playSound_linux, this);
        _soundThread->detach();
#endif
    }

private:
#ifdef __APPLE__
    void playSound_MAC()
    {
        CFURLRef fileURL = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)m_filename.c_str(), strlen(m_filename.c_str()), false);
        if (!fileURL)
        {
            std::cerr << "Failed to create file URL" << std::endl;
            return;
        }
        SystemSoundID soundID;
        OSStatus status = AudioServicesCreateSystemSoundID(fileURL, &soundID);
        CFRelease(fileURL);
        if (status != kAudioServicesNoError)
        {
            std::cerr << "Failed to create system sound ID: " << status << std::endl;
            return;
        }
        while (true)
        {
            AudioServicesPlaySystemSound(soundID);
            // Add a delay to control the loop speed (optional)
            std::this_thread::sleep_for(std::chrono::seconds(24));
        }
    }
#endif
#ifdef __linux__
    void playSound_linux()
    {
        unsigned int pcm, tmp, dir, rate, channels, seconds;
#ifdef SND_LIB_VERSION
        snd_pcm_t *pcm_handle;
        snd_pcm_hw_params_t *params;
        snd_pcm_uframes_t frames;
        char *buff;
        int buff_size, loops;

        rate = 44100;
        channels = 2;
        // seconds = 24;

        /* Open the PCM device in playback mode */
        if (pcm = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
            cout << "ERROR: Couldn't play sound on this system." << snd_strerror(pcm) << endl;
        else
        {
            /* Allocate parameters object and fill it with default values*/
            snd_pcm_hw_params_alloca(&params);

            snd_pcm_hw_params_any(pcm_handle, params);
            // set params
            snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
            snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
            snd_pcm_hw_params_set_channels(pcm_handle, params, channels);
            snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0);
            snd_pcm_hw_params(pcm_handle, params);
            snd_pcm_hw_params_get_period_size(params, &frames, 0);

            buff_size = frames * channels * 2 /* 2 -> sample size */;
            buff = (char *)malloc(buff_size);

            snd_pcm_hw_params_get_period_time(params, &tmp, NULL);
            int fd = open(m_filename.c_str(), O_RDONLY);
            while (true)
            {
                int bytes_read = read(fd, buff, buff_size);
                if (bytes_read == 0)
                {
                    // End of file, reset file pointer to the beginning
                    lseek(fd, 0, SEEK_SET);
                    continue;
                }
                if (bytes_read < 0)
                {
                    // Error reading file
                    cout << "ERROR: Couldn't read sound file." << endl;
                    break;
                }
                snd_pcm_writei(pcm_handle, buff, frames);
            }

            snd_pcm_drain(pcm_handle);
            snd_pcm_close(pcm_handle);
            free(buff);
        }

#endif
    }
#endif
};

class AbstractBoard : public Fl_Widget
{
public:
    virtual int getDestination(int pos) = 0;

    void draw(){};

    virtual std::pair<std::pair<int, int>, std::pair<int, int>> getPixelCoordinates(int pos) = 0;

protected:
    AbstractBoard(int x, int y, int w, int h, const char *label = nullptr)
        : Fl_Widget(x, y, w, h, label)
    {
    }

    virtual void embedLogic() = 0;
};

class SnakeLadderBoardWidget : public AbstractBoard
{
public:
    ~SnakeLadderBoardWidget()
    {
        delete m_boardBox;
        delete m_boardPNG;
    }

    static SnakeLadderBoardWidget *getInstance(int x, int y, int w, int h, const char *label = nullptr)
    {
        if (_instance == nullptr)
        {
            _instance = new SnakeLadderBoardWidget(x, y, w, h, label);
        }
        return _instance;
    }

    int getDestination(int pos) override // query the array to return the actual destination of player
    {
        return positionMap[pos - 1] + 1;
    }

    Coord getPixelCoordinates(int pos) override
    {
        auto p = positionMap[pos - 1];
        auto p1 = std::make_pair(squares[p].x + 5, squares[p].y + 5);
        auto p2 = std::make_pair(SQUARE_SIZE - 20, SQUARE_SIZE - 20);
        return std::make_pair(p1, p2);
    }
    void draw() override
    {
        m_boardBox->image(m_boardPNG);

        m_boardBox->size(m_boardPNG->w(), m_boardPNG->h());
    }

private:

   static SnakeLadderBoardWidget *_instance;
    Fl_Box *m_boardBox;
    Fl_PNG_Image *m_boardPNG;
    std::array<int, 100> positionMap;

    struct Square
    {
        int x, y;
        int number;
        int destination;
        Square(int x, int y, int number, int destination = -1) : x(x), y(y), number(number), destination(destination) {}
    };
    std::vector<Square> squares;

    SnakeLadderBoardWidget(int x, int y, int w, int h, const char *label = nullptr) : AbstractBoard(x, y, w, h, label)
    {
        embedLogic();
        m_boardBox = new Fl_Box(0, 15, 0, 0, "");
        m_boardPNG = new Fl_PNG_Image("board.png");
    }

    void embedLogic() override
    {
        for (size_t i = 0; i < positionMap.max_size(); i++)
        {
            positionMap[i] = i;
        }   
        positionMap[3] = 55;
        positionMap[11] = 49;
        positionMap[13] = 54;
        positionMap[21] = 57;
        positionMap[27] = 9;
        positionMap[36] = 2;
        positionMap[40] = 78;
        positionMap[47] = 15;
        positionMap[53] = 87;
        positionMap[74] = 31;
        positionMap[93] = 70;
        positionMap[95] = 41;
        bool reverse = false;
        int square_number = 1;
        for (int i = BOARD_SIZE - 1; i >= 0; --i)
        {
            if (reverse)
            {
                for (int j = BOARD_SIZE - 1; j >= 0; --j)
                {
                    squares.push_back(Square(j * SQUARE_SIZE + BORDER_X, i * SQUARE_SIZE + BORDER_Y, square_number++));
                }
            }
            else
            {
                for (int j = 0; j < BOARD_SIZE; ++j)
                {
                    squares.push_back(Square(j * SQUARE_SIZE + BORDER_X, i * SQUARE_SIZE + BORDER_Y, square_number++));
                }
            }
            reverse = !reverse;
        }
    }
};

SnakeLadderBoardWidget *SnakeLadderBoardWidget::_instance = nullptr;


class ScoreBoardWidget : public Fl_Widget
{
public:
    ~ScoreBoardWidget()
    {
        delete msgBox;
    }

    static ScoreBoardWidget *getInstance(int x, int y, int w, int h, const char *label = nullptr)
    {
        if (_instance == nullptr)
        {
            _instance = new ScoreBoardWidget(x, y, w, h, label);
        }
        return _instance;
    }

    void setMessage(std::string message)
    {
        msg = message;
    }

    void draw() override
    {
        msgBox->copy_label(msg.c_str());
    }

private:

    static ScoreBoardWidget *_instance;
    std::string msg;
    Fl_Box *msgBox;

    ScoreBoardWidget(int x, int y, int w, int h, const char *label = nullptr)
        : Fl_Widget(x, y, w, h, label)
    {
        msgBox = new Fl_Box(x + w + 30, 10, 100, 20, "");
        msgBox->labelfont(FL_HELVETICA_BOLD);
        msgBox->color(FL_WHITE); // Set the background colomsgBox
        msgBox->labelsize(18);
        msg = "";
    }
};

ScoreBoardWidget *ScoreBoardWidget::_instance = nullptr;

class BoardGameWidget : public Fl_Widget
{
public:

    BoardGameWidget(int x, int y, int w, int h, Fl_Window *_window, const char *label = nullptr)
        : window(_window), Fl_Widget(x, y, w, h, label), gameDone(false)
    {
        sfx = new SnakeLadderGameSound(MUSICFILENAME);
    }

    ~BoardGameWidget()
    {
        delete sfx;
        delete gameBoard;
        delete dice;
        delete scoreboard;
    }

    void CreateGame()
    {
        gameBoard = CreateBoard();
        dice = CreateDice();
        scoreboard = CreateScoreBoard();
        AddPlayer();
        window->end();
    }

    int play()
    {
        // Start playing the sound in a separate thread
        window->show();
        sfx->playSound();
        return Fl::run(); // Start FLTK event loop
    }

    void draw()
    {
    }
    virtual int handle(int event) = 0;

protected:
    AbstractBoard *gameBoard;
    DiceWidget *dice;
    ScoreBoardWidget *scoreboard;
    SoundDriver *sfx;
    Fl_Window *window;
    bool gameDone;
    std::string rollStr;

    virtual AbstractBoard *CreateBoard() = 0;
    virtual DiceWidget *CreateDice() = 0;
    virtual ScoreBoardWidget *CreateScoreBoard() = 0;
    virtual PlayerWidget *CreatePlayer(uint col = FL_RED) = 0;
    virtual void AddPlayer() = 0;
    virtual void PositionPlayersOnBoard(Coord A, Coord B, PlayerWidget *pA, PlayerWidget *pB) = 0;

};

class SnakeLadderBoardGameWidget : public BoardGameWidget
{
public:

    SnakeLadderBoardGameWidget(int x, int y, int w, int h, Fl_Window *_window, const char *label = nullptr)
        : BoardGameWidget(x, y, w, h, _window, label), turn(false), currentPlayerColor(RED)
    {
    }

    ~SnakeLadderBoardGameWidget()
    {
        BoardGameWidget::~BoardGameWidget();
        delete playerA;
        delete playerB;
        delete currentPlayer;
    }

    void AddPlayer() override
    {
        playerA = CreatePlayer();
        playerB = CreatePlayer(FL_DARK_GREEN);
        auto coords = gameBoard->getPixelCoordinates(1);
        playerA->updateCoordinates(coords.first.first - 5, coords.first.second, coords.second.first * 0.66, coords.second.second * 0.66);
        playerB->updateCoordinates(coords.first.first + 25, coords.first.second, coords.second.first * 0.66, coords.second.second * 0.66);
        currentPlayer = playerA;
    }

    int handle(int event) override
    {
        if (!gameDone && event == FL_PUSH)
        {
            window->redraw();
            // get current player
            auto pos = currentPlayer->getPosition();

            // get dice roll
            auto _throw = dice->roll();
            // check bound
            if (pos + _throw > 100)
            {
                std::string whoseTurn = (currentPlayerColor == GREEN) ? "Green: " : "Red: ";
                rollStr = whoseTurn + "Dice roll: " + std::to_string(_throw);
                scoreboard->setMessage(rollStr);
            }
            else
            {
                // check destination from board
                auto newpos = gameBoard->getDestination(pos + _throw);
                if (newpos == 100)
                {
                    std::string whoWin = (currentPlayerColor == GREEN) ? "Green wins" : "Red wins";
                    rollStr = "\nCongratulations!!\n" + whoWin;
                    scoreboard->setMessage(rollStr);
                    gameDone = true;
                }
                else
                {
                    // update scoreboard
                    if (newpos > pos + _throw)
                    {
                        rollStr = "Wow!!great luck";
                    }
                    else if (newpos < pos + _throw)
                    {
                        rollStr = "Oops!! bad luck";
                    }
                    else
                    {
                        std::string whoseTurn = (currentPlayerColor == GREEN) ? "Green: " : "Red: ";
                        rollStr = whoseTurn + "Dice roll: " + std::to_string(_throw);
                    }
                    scoreboard->setMessage(rollStr);
                }
                currentPlayer->updatePosition(newpos);
            }

            // Store current game state
            coordinate_playerA = gameBoard->getPixelCoordinates(playerA->getPosition());
            coordinate_playerB = gameBoard->getPixelCoordinates(playerB->getPosition());
            PositionPlayersOnBoard(coordinate_playerA, coordinate_playerB, playerA, playerB);

            turn = !turn;
            currentPlayer = turn ? playerB : playerA;
            currentPlayerColor = turn ? GREEN : RED;
            return 1;
        }

        return Fl_Widget::handle(event);
    }

private:

    bool turn;

    PlayerWidget *playerA, *playerB, *currentPlayer;

    Coord coordinate_playerA, coordinate_playerB;

    AbstractBoard *CreateBoard() override
    {
        SnakeLadderBoardWidget *gameBoard = SnakeLadderBoardWidget::getInstance(0, 0, WINDOW_WIDTH - 200, WINDOW_HEIGHT);
        return gameBoard;
    }

    DiceWidget *CreateDice() override
    {
        DiceWidget *dice = DiceWidget::getInstance(0, 0, WINDOW_WIDTH - 200, WINDOW_HEIGHT);
        return dice;
    }

    ScoreBoardWidget *CreateScoreBoard() override
    {
        ScoreBoardWidget *scoreboard = ScoreBoardWidget::getInstance(0, 0, WINDOW_WIDTH - 200, WINDOW_HEIGHT);
        return scoreboard;
    }

    PlayerWidget *CreatePlayer(uint col = FL_RED) override
    {
        PlayerWidget *pl = new PlayerWidget(0, 0, WINDOW_WIDTH - 200, WINDOW_HEIGHT, col);
        return pl;
    }

    void PositionPlayersOnBoard(Coord cA, Coord cB, PlayerWidget *pA, PlayerWidget *pB) override
    {
        auto xyA = cA.first;
        auto xyB = cB.first;
        if (xyA.first == xyB.first && xyA.second == xyB.second)
        {
            pA->updateCoordinates(cA.first.first - 5, cA.first.second, cA.second.first * 0.66, cA.second.second * 0.66);
            pB->updateCoordinates(cB.first.first + 25, cB.first.second, cB.second.first * 0.66, cB.second.second * 0.66);
        }
        else
        {
            pA->updateCoordinates(cA.first.first, cA.first.second, cA.second.first, cA.second.second);
            pB->updateCoordinates(cB.first.first, cB.first.second, cB.second.first, cB.second.second);
        }
    }
    
    enum PLAYER
    {
        RED,
        GREEN,
        BLUE
    };
    PLAYER currentPlayerColor;
};

int main()
{
    Fl_Window *window = new Fl_Window(WINDOW_WIDTH, WINDOW_HEIGHT, "Snake and Ladder");
    BoardGameWidget *game = new SnakeLadderBoardGameWidget(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, window);
    game->CreateGame();
    return game->play();
}

#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QTimer>
#include <QFont>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <linux/can.h>
#include <linux/can/isotp.h>

#include <cstdint>
#include <vector>

class AutoDiagWindow : public QMainWindow
{
public:
    AutoDiagWindow()
    {
        setWindowTitle("AutoDiag Linux - UDS Diagnostics");
        resize(900, 650);

        testerPresentTimer.setInterval(2000);
        connect(&testerPresentTimer, &QTimer::timeout,
                this, [this]() {
            sendTesterPresent();
        });

        auto *central = new QWidget(this);
        auto *mainLayout = new QVBoxLayout(central);

        auto *title = new QLabel("AUTODIAG LINUX");
        QFont titleFont = title->font();
        titleFont.setPointSize(22);
        titleFont.setBold(true);
        title->setFont(titleFont);

        auto *subtitle =
            new QLabel("Automotive UDS Diagnostic Interface");
        subtitle->setStyleSheet("color: #64748b;");

        mainLayout->addWidget(title);
        mainLayout->addWidget(subtitle);

        auto *connectionGroup = new QGroupBox("CAN Connection");
        auto *connectionLayout = new QHBoxLayout(connectionGroup);

        auto *interfaceLabel = new QLabel("Interface:");
        interfaceBox = new QComboBox();
        interfaceBox->addItem("vcan0");
        interfaceBox->setMinimumWidth(150);

        connectButton = new QPushButton("Connect");
        statusLabel = new QLabel("Disconnected");
        statusLabel->setStyleSheet(
            "color: #dc2626; font-weight: bold;");

        connectionLayout->addWidget(interfaceLabel);
        connectionLayout->addWidget(interfaceBox);
        connectionLayout->addWidget(connectButton);
        connectionLayout->addWidget(statusLabel);
        connectionLayout->addStretch();


        mainLayout->addWidget(connectionGroup);

        auto *quickGroup = new QGroupBox("UDS Quick Actions");
        auto *quickLayout = new QGridLayout(quickGroup);

        auto addQuickAction = [this, quickLayout](
            const QString &label, const QString &request,
            int row, int column) {
            auto *button = new QPushButton(label);
            button->setEnabled(false);
            quickLayout->addWidget(button, row, column);
            quickActionButtons.push_back(button);

            connect(button, &QPushButton::clicked,
                    this, [this, request]() {
                requestInput->setText(request);
                sendRequest();
            });
        };

        addQuickAction("Default Session", "10 01", 0, 0);
        addQuickAction("Extended Session", "10 03", 0, 1);
        addQuickAction("Request Security Seed", "27 01", 1, 0);
        addQuickAction("Unlock (Demo Key)", "27 02 B7 F7 C8 09", 1, 1);
        addQuickAction("Read VIN (F190)", "22 F1 90", 2, 0);
        addQuickAction("Read Protected DID (F1A0)", "22 F1 A0", 2, 1);
        addQuickAction("Read Calibration (F1A1)", "22 F1 A1", 3, 0);

        mainLayout->addWidget(quickGroup);

        auto *requestGroup = new QGroupBox("UDS Diagnostic Request");
        auto *requestLayout = new QVBoxLayout(requestGroup);

        auto *requestLabel =
            new QLabel("Request bytes (hexadecimal):");
        auto *requestRow = new QHBoxLayout();

        requestInput = new QLineEdit();
        requestInput->setPlaceholderText("Example: 22 F1 90");
        requestInput->setText("22 F1 90");

        sendButton = new QPushButton("Send Request");
        sendButton->setMinimumWidth(130);
        sendButton->setEnabled(false);

        requestRow->addWidget(requestInput);
        requestRow->addWidget(sendButton);

        requestLayout->addWidget(requestLabel);
        requestLayout->addLayout(requestRow);
        mainLayout->addWidget(requestGroup);

        auto *responseGroup = new QGroupBox("ECU Response");
        auto *responseLayout = new QVBoxLayout(responseGroup);

        responseView = new QTextEdit();
        responseView->setReadOnly(true);
        responseView->setPlaceholderText(
            "ECU diagnostic responses will appear here.");

        responseLayout->addWidget(responseView);
        mainLayout->addWidget(responseGroup, 1);

        auto *logGroup = new QGroupBox("Activity Log");
        auto *logLayout = new QVBoxLayout(logGroup);

        logView = new QTextEdit();
        logView->setReadOnly(true);
        logView->setMaximumHeight(100);
        logView->setPlaceholderText(
            "Connection and diagnostic events will appear here.");

        logLayout->addWidget(logView);
        mainLayout->addWidget(logGroup);

        setCentralWidget(central);

        connect(connectButton, &QPushButton::clicked,
                this, [this]() {
            if (socketFd >= 0)
                disconnectCan();
            else
                connectCan();
        });

        connect(sendButton, &QPushButton::clicked,
                this, [this]() {
            sendRequest();
        });
    }

    ~AutoDiagWindow() override
    {
        disconnectCan();
    }

private:
    QTimer testerPresentTimer;
    int socketFd = -1;

    QSocketNotifier *readNotifier = nullptr;

    QComboBox *interfaceBox = nullptr;
    QPushButton *connectButton = nullptr;
    QPushButton *sendButton = nullptr;
    std::vector<QPushButton *> quickActionButtons;
    QLabel *statusLabel = nullptr;
    QLineEdit *requestInput = nullptr;
    QTextEdit *responseView = nullptr;
    QTextEdit *logView = nullptr;

    void log(const QString &message)
    {
        logView->append(message);
    }

    void sendTesterPresent()
    {
        if (socketFd < 0)
            return;

        const uint8_t request[] = {0x3E, 0x80};

        ssize_t sent = write(socketFd, request, sizeof(request));

        if (sent != static_cast<ssize_t>(sizeof(request))) {
            log("TesterPresent transmission failed: " +
                QString::fromLocal8Bit(strerror(errno)));
            return;
        }

        log("TX: 3E 80 (automatic TesterPresent)");
    }

    void connectCan()
    {
        const QByteArray interfaceName =
            interfaceBox->currentText().toLocal8Bit();

        socketFd = socket(PF_CAN, SOCK_DGRAM, CAN_ISOTP);

        if (socketFd < 0) {
            log("Socket error: " +
                QString::fromLocal8Bit(strerror(errno)));
            return;
        }

        int flags = fcntl(socketFd, F_GETFL, 0);

        if (flags < 0 ||
            fcntl(socketFd, F_SETFL, flags | O_NONBLOCK) < 0) {
            log("Socket configuration error: " +
                QString::fromLocal8Bit(strerror(errno)));
            disconnectCan();
            return;
        }

        struct ifreq ifr {};
        strncpy(ifr.ifr_name, interfaceName.constData(),
                IFNAMSIZ - 1);

        if (ioctl(socketFd, SIOCGIFINDEX, &ifr) < 0) {
            log("Interface error: " +
                QString::fromLocal8Bit(strerror(errno)));
            disconnectCan();
            return;
        }

        struct sockaddr_can addr {};
        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;
        addr.can_addr.tp.tx_id = 0x700;
        addr.can_addr.tp.rx_id = 0x708;

        if (bind(socketFd,
                 reinterpret_cast<struct sockaddr *>(&addr),
                 sizeof(addr)) < 0) {
            log("ISO-TP bind error: " +
                QString::fromLocal8Bit(strerror(errno)));
            disconnectCan();
            return;
        }

        readNotifier = new QSocketNotifier(
            socketFd, QSocketNotifier::Read, this);

        connect(readNotifier, &QSocketNotifier::activated,
                this, [this](auto, auto) {
            receiveResponse();
        });

        statusLabel->setText("Connected");
        statusLabel->setStyleSheet(
            "color: #16a34a; font-weight: bold;");

        connectButton->setText("Disconnect");
        sendButton->setEnabled(true);
        for (auto *button : quickActionButtons)
            button->setEnabled(true);

        log("Connected to " + QString::fromLocal8Bit(
            interfaceName) + " using ISO-TP.");
        log("Tester TX: 0x700 | ECU RX: 0x708");

        testerPresentTimer.start();
        log("Automatic TesterPresent enabled (every 2 seconds).");
    }

    void disconnectCan()
    {
        testerPresentTimer.stop();

        if (readNotifier) {
            delete readNotifier;
            readNotifier = nullptr;
        }

        if (socketFd >= 0) {
            ::close(socketFd);
            socketFd = -1;
        }

        if (statusLabel) {
            statusLabel->setText("Disconnected");
            statusLabel->setStyleSheet(
                "color: #dc2626; font-weight: bold;");
        }

        if (connectButton)
            connectButton->setText("Connect");

        if (sendButton)
            sendButton->setEnabled(false);

        for (auto *button : quickActionButtons)
            button->setEnabled(false);

        if (logView)
            log("CAN connection closed.");
    }

    void sendRequest()
    {
        if (socketFd < 0)
            return;

        const QStringList tokens =
            requestInput->text().split(
                QRegularExpression("\\s+"),
                Qt::SkipEmptyParts);

        if (tokens.isEmpty()) {
            log("Error: Enter a UDS request.");
            return;
        }

        if (tokens.size() > 4095) {
            log("Error: Request exceeds ISO-TP payload limit.");
            return;
        }

        std::vector<uint8_t> request;

        for (const QString &token : tokens) {
            if (token.size() > 2 ||
                !QRegularExpression(
                    "^[0-9A-Fa-f]{1,2}$").match(token).hasMatch()) {
                log("Invalid hexadecimal byte: " + token);
                return;
            }

            bool ok = false;
            unsigned int value = token.toUInt(&ok, 16);

            if (!ok || value > 0xFF) {
                log("Invalid request byte: " + token);
                return;
            }

            request.push_back(
                static_cast<uint8_t>(value));
        }

        ssize_t sent = write(
            socketFd, request.data(), request.size());

        if (sent < 0) {
            log("Send error: " +
                QString::fromLocal8Bit(strerror(errno)));
            return;
        }

        if (static_cast<size_t>(sent) != request.size()) {
            log("Error: Incomplete ISO-TP request transmission.");
            return;
        }

        QString hex;

        for (uint8_t byte : request)
            hex += QString("%1 ").arg(
                static_cast<unsigned int>(byte), 2, 16,
                QChar('0')).toUpper();

        log("TX: " + hex.trimmed());
        responseView->clear();
        responseView->append("Waiting for ECU response...");
    }

    void receiveResponse()
    {
        uint8_t response[4095];

        ssize_t received = read(
            socketFd, response, sizeof(response));

        if (received < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                log("Receive error: " +
                    QString::fromLocal8Bit(strerror(errno)));
            return;
        }

        if (received == 0)
            return;

        QString hex;

        for (ssize_t i = 0; i < received; ++i) {
            hex += QString("%1 ").arg(
                static_cast<unsigned int>(response[i]),
                2, 16, QChar('0')).toUpper();
        }

        responseView->clear();
        responseView->append("Raw response: " + hex.trimmed());
        log("RX: " + hex.trimmed());

        if (received >= 3 &&
            response[0] == 0x62 &&
            response[1] == 0xF1 &&
            response[2] == 0x90) {

            QString vin = QString::fromLatin1(
                reinterpret_cast<const char *>(response + 3),
                static_cast<qsizetype>(received - 3));

            responseView->append("\nVIN: " + vin);
            log("VIN decoded successfully.");
        }
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    AutoDiagWindow window;
    window.show();

    return app.exec();
}

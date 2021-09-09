#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    myTimer = new QTimer(this);
    mySerial = new QSerialPort(this);
    mySettings = new SettingsDialog();
    myPaintBox = new QPaintBox(0,0,ui->widget);
    estado = new QLabel;
    estado->setText("Desconectado............");
    ui->statusbar->addWidget(estado);
    ui->actionDesconectar->setEnabled(false);
    //ui->widget->setVisible(false);
    estadoProtocolo=START;
    ui->pushButtonEnviar->setEnabled(false);
    estadoComandos=ALIVE;

    ///Conexión de eventos
    connect(mySerial,&QSerialPort::readyRead,this, &MainWindow::dataRecived );
    connect(myTimer, &QTimer::timeout,this, &MainWindow::myTimerOnTime);
    connect(ui->actionEscaneo_de_Puertos, &QAction::triggered, mySettings, &SettingsDialog::show);
    connect(ui->actionConectar,&QAction::triggered,this, &MainWindow::openSerialPort);
    connect(ui->actionDesconectar, &QAction::triggered, this, &MainWindow::closeSerialPort);
    connect(ui->actionSalir,&QAction::triggered,this,&MainWindow::close );

    myTimer->start(10);

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openSerialPort()
{
    SettingsDialog::Settings p = mySettings->settings();
    mySerial->setPortName(p.name);
    mySerial->setBaudRate(p.baudRate);
    mySerial->setDataBits(p.dataBits);
    mySerial->setParity(p.parity);
    mySerial->setStopBits(p.stopBits);
    mySerial->setFlowControl(p.flowControl);
    mySerial->open(QSerialPort::ReadWrite);
    if(mySerial->isOpen()){
        ui->actionConectar->setEnabled(false);
        ui->actionDesconectar->setEnabled(true);
        ui->pushButtonEnviar->setEnabled(true);
        estado->setText(tr("Conectado a  %1 : %2, %3, %4, %5, %6  %7")
                                         .arg(p.name).arg(p.stringBaudRate).arg(p.stringDataBits)
                                         .arg(p.stringParity).arg(p.stringStopBits).arg(p.stringFlowControl).arg(p.fabricante));
    }
    else{
        QMessageBox::warning(this,"Menu Conectar","No se pudo abrir el puerto Serie!!!!");
    }


}

void MainWindow::closeSerialPort()
{
    if(mySerial->isOpen()){
        mySerial->close();
        ui->actionDesconectar->setEnabled(false);
        ui->actionConectar->setEnabled(true);
        estado->setText("Desconectado................");
        ui->pushButtonEnviar->setEnabled(false);
    }
    else{
         estado->setText("Desconectado................");
         ui->pushButtonEnviar->setEnabled(false);
    }

}

void MainWindow::myTimerOnTime()
{

    if(rxData.timeOut!=0){
        rxData.timeOut--;
    }else{
        estadoProtocolo=START;
    }
}

void MainWindow::dataRecived()
{
    unsigned char *incomingBuffer;
    int count;

    count = mySerial->bytesAvailable();

    if(count<=0)
        return;

    incomingBuffer = new unsigned char[count];

    mySerial->read((char *)incomingBuffer,count);

    QString str="";

    for(int i=0; i<=count; i++){
        if(isalnum(incomingBuffer[i]))
            str = str + QString("%1").arg((char)incomingBuffer[i]);
        else
            str = str +"{" + QString("%1").arg(incomingBuffer[i],2,16,QChar('0')) + "}";
    }
    ui->textBrowser->append("MBED-->PC (" + str + ")");
    rxData.timeOut=5;
    for(int i=0;i<count; i++){
        switch (estadoProtocolo) {
            case START:
                if (incomingBuffer[i]=='U'){
                    estadoProtocolo=HEADER_1;
                    rxData.cheksum=0;
                }
                break;
            case HEADER_1:
                if (incomingBuffer[i]=='N')
                   estadoProtocolo=HEADER_2;
                else{
                    i--;
                    estadoProtocolo=START;
                }
                break;
            case HEADER_2:
                if (incomingBuffer[i]=='E')
                    estadoProtocolo=HEADER_3;
                else{
                    i--;
                   estadoProtocolo=START;
                }
                break;
        case HEADER_3:
            if (incomingBuffer[i]=='R')
                estadoProtocolo=NBYTES;
            else{
                i--;
               estadoProtocolo=START;
            }
            break;
            case NBYTES:
                rxData.nBytes=incomingBuffer[i];
               estadoProtocolo=TOKEN;
                break;
            case TOKEN:
                if (incomingBuffer[i]==':'){
                   estadoProtocolo=PAYLOAD;
                    rxData.cheksum='U'^'N'^'E'^'R'^ rxData.nBytes^':';
                    rxData.payLoad[0]=rxData.nBytes;
                    rxData.index=1;
                }
                else{
                    i--;
                    estadoProtocolo=START;
                }
                break;
            case PAYLOAD:
                if (rxData.nBytes>1){
                    rxData.payLoad[rxData.index++]=incomingBuffer[i];
                    rxData.cheksum^=incomingBuffer[i];
                }
                rxData.nBytes--;
                if(rxData.nBytes==0){
                    estadoProtocolo=START;
                    if(rxData.cheksum==incomingBuffer[i]){
                        decodeData();
                    }
                }
                break;
            default:
                estadoProtocolo=START;
                break;
        }
    }
    delete [] incomingBuffer;
}

void MainWindow::decodeData()
{
    QString str="";
    for(int a=1; a < rxData.index; a++){
        switch (rxData.payLoad[1]) {
            case ALIVE:
                str = "MBED-->PC *ID Válido* (¡¡¡¡¡ESTOY VIVO!!!!!!)";
                break;
            case GET_LEDS:
                str = "MBED-->PC *ID Válido* (¡¡¡¡¡ESTADO DE LOS LEDS!!!!!!)";
                myWord.ui8[0]=rxData.payLoad[2];
                myWord.ui8[1]=rxData.payLoad[3];
                getLedsCanvas(myWord.ui16[0]);
                break;
            case SET_LEDS:
                str = "MBED-->PC *ID Válido* (¡¡¡¡¡LEDS SETEADOS CORRECTAMENTE!!!!!!)";
                break;
            default:
                str=((char *)rxData.payLoad);
                str= ("MBED-->PC *ID Invalido * (" + str + ")");
                break;
        }
    }
    ui->textBrowser->append(str);

}


void MainWindow::on_pushButtonEnviar_clicked()
{
    uint16_t auxleds=0;
    txData.index=0;
    txData.payLoad[txData.index++]='U';
    txData.payLoad[txData.index++]='N';
    txData.payLoad[txData.index++]='E';
    txData.payLoad[txData.index++]='R';
    txData.payLoad[txData.index++]=0;
    txData.payLoad[txData.index++]=':';

    switch (estadoComandos) {
    case ALIVE:
        txData.payLoad[txData.index++]=ALIVE;
        txData.payLoad[NBYTES]=0x02;
        break;
    case GET_LEDS:
        txData.payLoad[txData.index++]=GET_LEDS;
        txData.payLoad[NBYTES]=0x02;
        break;
    case SET_LEDS:
        txData.payLoad[txData.index++]=SET_LEDS;
        if(ui->checkBoxLed1->isChecked()){
            if(ui->radioButtonOn->isChecked())
                auxleds |= 1 <<3;
            else
                auxleds &= ~(1<<3);
        }
        if(ui->checkBoxLed2->isChecked()){
            if(ui->radioButtonOn->isChecked())
                auxleds |= 1 <<2;
            else
                auxleds &= ~(1<<2);
        }
        if(ui->checkBoxLed3->isChecked()){
            if(ui->radioButtonOn->isChecked())
                auxleds |= 1 <<1;
            else
                auxleds &= ~(1<<1);
        }
        if(ui->checkBoxLed4->isChecked()){
            if(ui->radioButtonOn->isChecked())
                auxleds |= 1 <<0;
            else
                auxleds &= ~(1<<0);
        }
        myWord.ui16[0]=auxleds;
        txData.payLoad[txData.index++]=myWord.ui8[0];
        txData.payLoad[txData.index++]=myWord.ui8[1];
        txData.payLoad[NBYTES]=0x04;
        break;
    case OTHERS:
        break;
    default:
        break;
    }
   txData.cheksum=0;
   for(int a=0 ;a<txData.index;a++)
       txData.cheksum^=txData.payLoad[a];
    txData.payLoad[txData.index]=txData.cheksum;
    if(mySerial->isWritable()){
        mySerial->write((char *)txData.payLoad,txData.payLoad[NBYTES]+6);
    }

    QString str="";
    for(int i=0; i<=txData.index; i++){
        if(isalnum(txData.payLoad[i]))
            str = str + QString("%1").arg((char)txData.payLoad[i]);
        else
            str = str +"{" + QString("%1").arg(txData.payLoad[i],2,16,QChar('0')) + "}";
    }
    ui->textBrowser->append("PC-->MBED (" + str + ")");

}

void MainWindow::on_comboBox_currentIndexChanged(int index)
{
    switch (index) {
        case 1:
            estadoComandos=ALIVE;
        break;
        case 2:
            estadoComandos=GET_LEDS;
        break;
        case 3:
            estadoComandos=SET_LEDS;
        break;
        case 4:
            estadoComandos=OTHERS;
        break;
    default:
        ;
    }
}

void MainWindow::getLedsCanvas(uint16_t leds)
{
    QString strvaly;
    uint8_t ancho=60, alto=60;
    int16_t posx,posy, divide, auxled=0;
    QPen Pen;
    QPainter paint(myPaintBox->getCanvas());
    myPaintBox->getCanvas()->fill(Qt::gray);
    paint.setPen(Qt::black);
    auxled|=1<<3;
    if(auxled&leds)
        paint.setBrush(Qt::red);
    else
        paint.setBrush(Qt::darkGray);
    divide=myPaintBox->width()/4;
    posx=(divide/2)-(ancho/2);
    posy=(myPaintBox->height()/2);
    posy/=2;
    paint.drawEllipse(posx, posy,ancho,alto);
    posx+=divide;
    auxled=0;
    auxled|=1<<2;
    if(auxled&leds)
        paint.setBrush(Qt::blue);
    else
        paint.setBrush(Qt::darkGray);
    paint.drawEllipse(posx, posy,ancho,alto);
    posx+=divide;
    auxled=0;
    auxled|=1<<1;
    if(auxled&leds)
        paint.setBrush(Qt::yellow);
    else
        paint.setBrush(Qt::darkGray);
    paint.drawEllipse(posx, posy,ancho,alto);
    posx+=divide;
    auxled=0;
    auxled|=1<<0;
    if(auxled&leds)
        paint.setBrush(Qt::green);
    else
        paint.setBrush(Qt::darkGray);

    paint.drawEllipse(posx, posy,ancho,alto);

    Pen.setWidth(2);
    Pen.setColor(Qt::white);
    paint.setPen(Pen);
    paint.setRenderHint(QPainter::Antialiasing);
    paint.setFont(QFont("Arial",13,QFont::Bold));
    strvaly="LED_R";
    posx=(divide/2)-(ancho/2);
    posy=25;
    paint.drawText(posx,posy,strvaly);
    strvaly="LED_B";
    posx+=divide;
    paint.drawText(posx,posy,strvaly);
    strvaly="LED_Y";
    posx+=divide;
    paint.drawText(posx,posy,strvaly);
    strvaly="LED_G";
    posx+=divide;
    paint.drawText(posx,posy,strvaly);

    myPaintBox->update();

}
/*
void MainWindow::on_pushButton_clicked()
{
    getLedsCanvas(8);
}*/

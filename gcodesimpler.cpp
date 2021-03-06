#include "gcodesimpler.h"
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QChar>

static const quint8 prec = 3;

GCodeSimpler::GCodeSimpler(QObject *parent) :
    QObject(parent)
{
}

const QString GCodeSimpler::simplify(const QString &reference)
{
    QString identifier;
    int code = -1;
    QString line;
    QString data;
    QStringList list;
    list.clear();
    list = reference.split(" ", QString::SkipEmptyParts);
    //qDebug() << line;
    identifier = list.front();
    //qDebug() << identifier;
    if (identifier.startsWith("G"))
    {
        data = identifier.remove(0,1);
        code = data.toInt();
        //qDebug() << code;
        if (code==1 || code == 0)
        {
            //treat as a G-line
            //qDebug() << reference;
            for (int i = 1; i < list.count(); ++i)
            {
                //do extraction
                data = list.at(i);
                //char c = data.at(0).toLower().toAscii();
                char c = data.at(0).toLower().toLatin1();
                switch (c)
                {
                case 'x':

                    x=data.remove(0,1).toDouble();
                    break;
                case 'y':
                    y=data.remove(0,1).toDouble();
                    break;
                case 'z':
                    z=data.remove(0,1).toDouble();
                    if (z_ground)
                    {
                        ref_z = z;
                        z_ground = false;
                    }
                    //qDebug() << ref_z;
                    z -= ref_z;
                    break;
                case 'e':
                    e=data.remove(0,1).toDouble();
                    break;
                case 'f':
                    f=data.remove(0,1).toDouble()/60;
                    break;
                }
            }
            line = QString("%1 %2 %3 %4 %5").arg(QString::number(x,'f',prec)).arg(QString::number(y,'f',prec)).arg(QString::number(z,'f',prec)).arg(QString::number(f,'f',prec)).arg(QString::number(e,'f',prec));
        }
    }
 
    return line;
}

void GCodeSimpler::processGCode(const QStringList &filelist)
{
    unsigned int filecount = 0;
    foreach (QString filepath, filelist)
    {
        QString gCodeFilePath = filepath;
        consoleWrite(tr("Source file: %1").arg(gCodeFilePath));

        QFile inFile(gCodeFilePath);
        if (!inFile.open(QIODevice::Text | QIODevice::ReadOnly))
        {
            processingResult(false,tr("Source file not accessible."));
            continue;
        }

        QFileInfo inFileInfo(inFile);
        if (inFileInfo.completeSuffix() != inSuffix)
        {
            processingResult(false,tr("Not a GCode file."));
            continue;
        }

        QDir dir = inFileInfo.absoluteDir();
        QFileInfo inDirInfo(dir.path());
        if (!inDirInfo.isWritable())
        {
            processingResult(false,tr("Target folder not writable."));
            continue;
        }

        QDir::setCurrent(dir.path());
        QString xj3dpFilePath = QString("%1.%2").arg(inFileInfo.baseName()).arg(outSuffix);
        QFile outFile(xj3dpFilePath);
        // QFileInfo outFileInfo(outFile);

        // NOTE: this will overwrite existing file
        // TODO: need a auto renaming solution
        // if (outFile.exists())
        //    return finished(false,"Target file exists.");

        if (!outFile.open(/*QIODevice::Text | */QIODevice::WriteOnly))
        {
            consoleWrite(tr("Target file not writable."));
            finished(false,tr("Target file not writable."));
            inFile.close();
            continue;
        }

        //everything seems ready.
        consoleWrite(tr("Processing GCode..."));
        emit processing(inFileInfo.absoluteFilePath());

        QTextStream inStream(&inFile);
        QTextStream outStream(&outFile);

        QString inLine;
        //QString outLine;
        QStringList outList;
        QStringList layerList;

        QString data;

        clearPosition();

        // ugly code below
        // process gcode

        //TODO: refer to any GCode Parsing Library

        while (!inStream.atEnd())
        {
            inLine = inStream.readLine();
            if (inLine.contains("(") || inLine.contains(";") ||inLine.isEmpty())
                continue;
            data = simplify(inLine);
            if (!data.isEmpty())
            {
                //data = line;
                if ((z!=last_z) && (!first))
                {
                    //layer change
                    processLayerChange(outList, layerList);
                    data_count = 0;
                    layer_count++;
                }

                data_count++;
                layerList.append(data);
                last_z = z;
                first = false;
            }
                 //outStream << line << endl;
        }

        //process last layer
        processLayerChange(outList, layerList);

        // no layer_count++ at the very end so we do it right away
        data =  QString("layers=%1").arg(QString::number(layer_count+1));
        outList.prepend(data);

        //write to file
        //foreach is slow
        QStringList::const_iterator it;
        for (it = outList.begin();it != outList.end();++it)
        {
            outStream << *it << "\r\n";
        }

        //processing done.

        inFile.close();
        outFile.close();

        filecount++;
    }
    if (filecount > 0)
    {
        processingResult(true,tr("%1 GCode file(s) processing complete.").arg(QString::number(filecount)));
    }
    else
    {
        processingResult(false,tr("No file processed."));
    }

}

void GCodeSimpler::clearPosition()
{
    x=y=z=e=f=last_z=ref_z=0;
    layer_count=data_count=0;
    first = true;
    z_ground = true;
}

void GCodeSimpler::processLayerChange(QStringList &outList, QStringList &list)
{
    QString line;
    line = QString("%1").arg(QString::number(data_count));
    list.prepend(line);
    line = QString("layer %1 seg 1").arg(QString::number(layer_count));
    list.prepend(line);

    outList << list;
    list.clear();
}

void GCodeSimpler::consoleWrite(const QString & message)
{
    //qDebug("%s",string.toStdString().c_str());
    QTextStream out(stdout);
    out << message << endl;
}

void GCodeSimpler::processingResult(bool status, const QString &message)
{
    consoleWrite(message);
    emit finished(status, message);
}


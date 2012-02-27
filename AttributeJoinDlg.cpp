//include
#include "AttributeJoinDlg.h"
#include "ui_AttributeJoinDlg.h"
#include "csv.h"
#include "ErrorLog.h"

//QT include
#include <QFile>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>

//QGIS include
#include <QgsVectorFileWriter.h>
#include <QgsVectorLayer.h>
#include <QgsMapLayerRegistry.h>
#include <QgsVectorDataProvider.h>
#include <QgsField.h>
#include <QGisInterface.h>

#include <math.h>

// ���ڒ�`�e�[�u��
#define ROW_HEIGHT                  25
#define TBL_FIELD_COUNT             5
#define TBL_CSV_FIELD_NAME          0
#define TBL_ATTR_FIELD_NAME         1
#define TBL_ATTR_FIELD_TYPE         2
#define TBL_ATTR_FIELD_LENGTH       3
#define TBL_ATTR_FIELD_PRECISION    4

// �t�B�[���h�̌^
#define FIELD_TYPE_STRING           1
#define FIELD_TYPE_INT              2
#define FIELD_TYPE_REAL             3

// �t�B�[���h�̍ő�l
#define FIELD_TYPE_STRING_MAX       255
#define FIELD_TYPE_INT_MAX          10
#define FIELD_TYPE_REAL_MAX         20
#define FIELD_TYPE_PRECISION_MAX    5


#define FILE_ENCODING      tr("system")

AttributeJoinDlg::AttributeJoinDlg(QgisInterface* iface,QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AttributeJoinDlg)
{
    ui->setupUi(this);
    mIface = iface;

    m_errLog = new ErrorLog();

    // �_�C�A���O�̃R���g���[���{�b�N�X��ݒ�
    setWindowFlags(Qt::Dialog | Qt::WindowSystemMenuHint | Qt::MSWindowsFixedSizeDialogHint);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(joinProc()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(close()));
    connect(ui->cmdSelectCsvFile, SIGNAL(clicked()), this, SLOT(selectCsvFile()));
    connect(ui->cmdSelectShapeFile, SIGNAL(clicked()), this, SLOT(selectShapeFile()));
    connect(ui->cmdSelectUnjoinedFile, SIGNAL(clicked()), this, SLOT(selectUnjoinedFile()));
    connect(ui->cboJoinLayer, SIGNAL(currentIndexChanged(int)), this, SLOT(changeLayer(int)));
    connect(ui->radioComma, SIGNAL(clicked()), this, SLOT(changeCsvType()));
    connect(ui->radioTab, SIGNAL(clicked()), this, SLOT(changeCsvType()));

    initUi();
}

AttributeJoinDlg::~AttributeJoinDlg()
{
    delete m_errLog;
    delete ui;
}

void AttributeJoinDlg::initUi()
{
    ui->cboJoinLayer->clear();

    QMap<QString, QgsMapLayer*> value = QgsMapLayerRegistry::instance()->mapLayers();
    QMap<QString, QgsMapLayer * >::const_iterator ite;
    for(ite = value.constBegin(); ite != value.constEnd(); ite++) {
        QgsMapLayer *layer = ite.value();
        if(layer->type() == QgsMapLayer::VectorLayer) {
            ui->cboJoinLayer->addItem(layer->name(), layer->getLayerID());
        }
    }

    ui->cboJoinLayer->setCurrentIndex(-1);
}

void AttributeJoinDlg::changeLayer(int index)
{
    Q_UNUSED(index);

    ui->cboJoinLayerField->clear();

    QgsVectorLayer *vecLayer = getVectorLayerByName(ui->cboJoinLayer->currentText());
    if(vecLayer == NULL) return;

    QgsFieldMap attrMap = vecLayer->pendingFields();
    QgsFieldMap::iterator ite;
    QgsAttributeList attrList = vecLayer->pendingAllAttributesList();
    for(int i=0; i<attrList.count(); i++) {
        ite = attrMap.find(attrList.at(i));
        assert(ite != attrMap.end());
        QgsField field = *ite;
        ui->cboJoinLayerField->addItem(field.name());
    }
    ui->cboJoinLayerField->setCurrentIndex(-1);
}

// CSV�t�@�C���ݒ�
void AttributeJoinDlg::selectCsvFile()
{
    QString csvFileName = QFileDialog::getOpenFileName( NULL, tr("�t�@�C���I��"),
        tr(""), tr("CSV�t�@�C�� (*.csv);;TSV�t�@�C�� (*.tsv);;Text�t�@�C�� (*.txt);;�S��(*.*)"),
        NULL, QFileDialog::ReadOnly);
    if(csvFileName.isEmpty()) return;

    ui->txtCsvFileName->setText(csvFileName);

    loadCsvHeader(csvFileName);
}

void AttributeJoinDlg::loadCsvHeader(const QString& filename)
{
    QFile file( filename );
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        QMessageBox::critical(NULL, tr("�t�@�C���I�[�v���G���["), tr("�t�@�C���̃I�[�v���Ɏ��s���܂���\n%1").arg(filename));
        return;
    }

    // CSV����w�b�_���擾���ăR���{�ɐݒ肷��
    QString lineBuf = file.readLine();
    QStringList header = CSV::parseLine(lineBuf, ui->radioTab->isChecked());
    ui->cboCsvFileField->clear();
    for(int i=0; i<header.count(); i++) {
        ui->cboCsvFileField->addItem(header.at(i));
    }
    ui->cboCsvFileField->setCurrentIndex(-1);

    // ���ڃe�[�u����ݒ�
    ui->tblCsvField->setRowCount(ui->cboCsvFileField->count());
    ui->tblCsvField->setColumnCount(TBL_FIELD_COUNT);
    QStringList headerLabels;
    headerLabels << tr("�t�@�C�����ږ�") << tr("������̍��ږ�") << tr("�^") << tr("��") << tr("���x");
    ui->tblCsvField->setHorizontalHeaderLabels(headerLabels);

    // ���x����t���[���ł� setAutoFillBackground=true + setBackgroundRole�Ŕw�i�F�̕ύX���ł��邪
    // �e�L�X�g�{�b�N�X�ł͂Ȃ����ł��Ȃ��̂ŁA�p���b�g�̐F���̂��̂�ύX����
    QPalette	palette;
    palette = this->palette();
    palette.setColor(QPalette::Disabled, QPalette::Text, palette.color(QPalette::Active, QPalette::Text));

    for(int nrow=0; nrow<header.count(); nrow++) {

        QLineEdit *lineEdit;

        // ���ڂP : ���ږ�
        lineEdit = new QLineEdit(header.at(nrow), ui->tblCsvField);
        lineEdit->setFrame(false);
        lineEdit->setEnabled(false);
        ui->tblCsvField->setCellWidget(nrow, TBL_CSV_FIELD_NAME, lineEdit);
        lineEdit->setPalette(palette);

        // ���ڂQ : �n�����ږ�
        lineEdit = new QLineEdit(header.at(nrow), ui->tblCsvField);
        lineEdit->setFrame(false);
        ui->tblCsvField->setCellWidget(nrow, TBL_ATTR_FIELD_NAME, lineEdit);

        // ���ڂR : �f�[�^�^
        QComboBox *cbTest = new QComboBox(this);
        cbTest->setEnabled(true);
        cbTest->clear();
        cbTest->addItem(tr("�e�L�X�g�f�[�^"), QVariant((int)FIELD_TYPE_STRING));
        cbTest->addItem(tr("�����l"), QVariant((int)FIELD_TYPE_INT));
        cbTest->addItem(tr("�����_�t���l"), QVariant((int)FIELD_TYPE_REAL));
        cbTest->setCurrentIndex(0);
        ui->tblCsvField->setCellWidget(nrow, TBL_ATTR_FIELD_TYPE, cbTest);
        connect(cbTest, SIGNAL(currentIndexChanged(int)), this, SLOT(changeDataType(int)));

        // ���ڂS : ����
        lineEdit = new QLineEdit("80", ui->tblCsvField);
        lineEdit->setFrame(false);
        ui->tblCsvField->setCellWidget(nrow, TBL_ATTR_FIELD_LENGTH, lineEdit);

        // ���ڂT : ���x
        lineEdit = new QLineEdit("0", ui->tblCsvField);
        lineEdit->setFrame(false);
        QIntValidator *validator = new QIntValidator(0, FIELD_TYPE_PRECISION_MAX, lineEdit);
        lineEdit->setValidator(validator);
        ui->tblCsvField->setCellWidget(nrow, TBL_ATTR_FIELD_PRECISION, lineEdit);

        // �s����ݒ�
        ui->tblCsvField->setRowHeight(nrow, ROW_HEIGHT);
    }

    changeDataType(0);

    //	fit of column size at contents.
    ui->tblCsvField->resizeColumnsToContents();

    // ���A���x�̓t�B�b�g�T�C�Y�ł͍L������̂ŁA�������Ă���
    ui->tblCsvField->setColumnWidth(TBL_ATTR_FIELD_LENGTH, 40);
    ui->tblCsvField->setColumnWidth(TBL_ATTR_FIELD_PRECISION, 40);

    file.close();
}

void AttributeJoinDlg::changeCsvType()
{
    // ���[�h���Ȃ���
    QString filename = ui->txtCsvFileName->text().trimmed();
    if(filename.isEmpty()) return;

    loadCsvHeader(filename);
}

void AttributeJoinDlg::changeDataType(int index)
{
    Q_UNUSED(index);

    int             nrow, length;
    QComboBox       *comboBox;
    QLineEdit       *editLength, *editPrecision;
    QIntValidator   *validator;

    for(nrow=0; nrow<ui->tblCsvField->rowCount(); nrow++) {

        comboBox = dynamic_cast <QComboBox*>(ui->tblCsvField->cellWidget(nrow, TBL_ATTR_FIELD_TYPE));
        editLength = dynamic_cast <QLineEdit*>(ui->tblCsvField->cellWidget(nrow, TBL_ATTR_FIELD_LENGTH));
        editPrecision = dynamic_cast <QLineEdit*>(ui->tblCsvField->cellWidget(nrow, TBL_ATTR_FIELD_PRECISION));

        switch(comboBox->itemData(comboBox->currentIndex(), Qt::UserRole).toInt()) {
            case FIELD_TYPE_STRING:
                editPrecision->setEnabled(false);
                validator = new QIntValidator(0, FIELD_TYPE_STRING_MAX, editLength);
                editLength->setValidator(validator);
                break;
            case FIELD_TYPE_INT:
                editPrecision->setEnabled(false);
                length = editLength->text().toInt();
                if(length < 0 || length > FIELD_TYPE_INT_MAX) {
                    editLength->setText(tr("%1").arg(FIELD_TYPE_INT_MAX));
                }
                validator = new QIntValidator(0, FIELD_TYPE_INT_MAX, editLength);
                editLength->setValidator(validator);
                break;
            case FIELD_TYPE_REAL:
                editPrecision->setEnabled(true);
                length = editLength->text().toInt();
                if(length < 0 || length > FIELD_TYPE_REAL_MAX) {
                    editLength->setText(tr("%1").arg(FIELD_TYPE_REAL_MAX));
                }
                validator = new QIntValidator(0, FIELD_TYPE_REAL_MAX, editLength);
                editLength->setValidator(validator);
                break;
        }


    }
}

// shape�t�@�C���ݒ�
void AttributeJoinDlg::selectShapeFile()
{
    QString shapeFileName = QFileDialog::getSaveFileName( NULL,
                                                        tr("shape�t�@�C���I��"),
                                                        tr(""), tr("*.shp"), NULL, 0);
    if(shapeFileName.isEmpty()) return;

    ui->txtShapeFileName->setText(shapeFileName);
}

// ��������CSV�t�@�C���ݒ�
void AttributeJoinDlg::selectUnjoinedFile()
{
    QString unjoinedFileName = QFileDialog::getSaveFileName( NULL,
                                                        tr("CSV�t�@�C���I��"),
                                                        tr(""), tr("*.csv"), NULL, 0);
    if(unjoinedFileName.isEmpty()) return;

    ui->txtUnjoinedFileName->setText(unjoinedFileName);
}

// �����O�̏�������
bool AttributeJoinDlg::preJoinProc()
{
    QString         layerName;
    bool            flagOk;

    // CSV�t�@�C����
    m_csvFileName = ui->txtCsvFileName->text().trimmed();
    if(m_csvFileName.isEmpty()) {
        QMessageBox::critical(NULL, tr("���̓G���["), tr("�e�L�X�g�t�@�C�����w�肵�Ă�������"));
        ui->txtCsvFileName->setFocus();
        return false;
    }

    // Shape�t�@�C����
    m_shapeFileName = ui->txtShapeFileName->text().trimmed();
    if(m_shapeFileName.isEmpty()) {
        QMessageBox::critical(NULL, tr("���̓G���["), tr("Shape�t�@�C�����w�肵�Ă�������"));
        ui->txtShapeFileName->setFocus();
        return false;
    }

    // �G���[�t�@�C����
    m_unjoinedFileName = ui->txtUnjoinedFileName->text().trimmed();
    if(m_unjoinedFileName.isEmpty()) {
        QMessageBox::critical(NULL, tr("���̓G���["), tr("�G���[���X�g�t�@�C�����w�肵�Ă�������"));
        ui->txtUnjoinedFileName->setFocus();
        return false;
    }

    // �t�@�C�����̃`�F�b�N
    if( m_csvFileName == m_shapeFileName ||
        m_csvFileName == m_unjoinedFileName ||
        m_shapeFileName == m_unjoinedFileName) {
        QMessageBox::critical(NULL, tr("���̓G���["), tr("�e�L�X�g�t�@�C���A�o��Shape�A�����ł��Ȃ�����CSV���R�[�h���X�g�̂����ꂩ�ɓ����t�@�C�������w�肳��Ă��܂�"));
        return false;
    }

    // �G���[���O�̃I�[�v��
    if(!m_errLog->open(m_unjoinedFileName)) return false;

    // CSV�}�b�`���O�t�B�[���h
    m_csvKeyName = ui->cboCsvFileField->currentText();
    if(m_csvKeyName.isEmpty()) {
        QMessageBox::critical(NULL, tr("���̓G���["), tr("�e�L�X�g�t�@�C���̃}�b�`���O�t�B�[���h���w�肵�Ă�������"));
        ui->cboCsvFileField->setFocus();
        return false;
    }

    // ���C����
    layerName = ui->cboJoinLayer->currentText();
    if(layerName.isEmpty()) {
        QMessageBox::critical(NULL, tr("���̓G���["), tr("���C�����w�肵�Ă�������"));
        ui->cboJoinLayer->setFocus();
        return false;
    }

    // ���C���}�b�`���O�t�B�[���h
    m_layerKeyName = ui->cboJoinLayerField->currentText();
    if(m_layerKeyName.isEmpty()) {
        QMessageBox::critical(NULL, tr("���̓G���["), tr("���C���̃}�b�`���O�t�B�[���h���w�肵�Ă�������"));
        ui->cboJoinLayerField->setFocus();
        return false;
    }

    // �����̃��C���̑������ڂɐV�����������ڂ�ǉ����ĐV�����������X�g���쐬����
    m_vecLayer = getVectorLayerByName(layerName);
    QgsVectorDataProvider *vecProvider = m_vecLayer->dataProvider();
    m_orgFieldMap = vecProvider->fields();
    m_newFieldMap = m_orgFieldMap;
    QgsFieldMap::iterator ite;
    QgsField newField;

    //�t�B�[���h���d���̍ۂ̘A��
    int tempFieldCount = 0;


    for(int nrow=0; nrow<ui->tblCsvField->rowCount(); nrow++) {

        QString         fieldName;
        int             fieldLengthMax;
        QLineEdit       *lineEditName, *lineEditLength, *lineEditPrecision;
        QComboBox       *cboFieldType;

        // �t�B�[���h���̎擾
        lineEditName = dynamic_cast <QLineEdit*>(ui->tblCsvField->cellWidget(nrow, TBL_ATTR_FIELD_NAME));
        fieldName = lineEditName->text().trimmed();
        if(fieldName.isEmpty()) {
            QMessageBox::critical(NULL, tr("���̓G���["), tr("���ږ�����͂��Ă�������"));
            lineEditName->setFocus();
            return false;
        }

        // �������ږ����d�����Ă��Ȃ����`�F�b�N
        if(isExistField(m_newFieldMap, fieldName)) {
            //�d�����Ă���ꍇ�ɂ͕ʖ����w�肷��
            fieldName = "_dat" + QString("%1").arg(QString::number(tempFieldCount), 4, QChar('0'));
            ++tempFieldCount;
            lineEditName->setText(fieldName);

            //QMessageBox::critical(NULL, tr("���̓G���["), tr("���̍��ږ��͂��łɑ��݂��܂�"));
            //lineEditName->setFocus();
            //return false;
        }

        // �t�B�[���h�ɖ��O��ݒ�
        newField.setName(fieldName);

        // �^�C�v�̎擾
        cboFieldType = dynamic_cast <QComboBox*>(ui->tblCsvField->cellWidget(nrow, TBL_ATTR_FIELD_TYPE));
        switch(cboFieldType->itemData(cboFieldType->currentIndex(), Qt::UserRole).toInt()) {
            case FIELD_TYPE_STRING:
                newField.setType(QVariant::String);
                fieldLengthMax = FIELD_TYPE_STRING_MAX;
                break;
            case FIELD_TYPE_INT:
                newField.setType(QVariant::Int);
                fieldLengthMax = FIELD_TYPE_INT_MAX;
                break;

            case FIELD_TYPE_REAL:
                newField.setType(QVariant::Double);
                fieldLengthMax = FIELD_TYPE_REAL_MAX;
                break;
        }

        // ���ڒ��̎擾
        lineEditLength = dynamic_cast <QLineEdit*>(ui->tblCsvField->cellWidget(nrow, TBL_ATTR_FIELD_LENGTH));
        newField.setLength(lineEditLength->text().toInt(&flagOk));
        if(!flagOk || newField.length() <= 0 || newField.length() > fieldLengthMax) {
            QMessageBox::critical(NULL, tr("���̓G���["), tr("�K�؂ȃt�B�[���h�̒������w�肵�Ă��������B[1 �` %2]").arg(fieldLengthMax));
            lineEditLength->setFocus();
            return false;
        }

        // ���ڒ��̎擾
        lineEditPrecision = dynamic_cast <QLineEdit*>(ui->tblCsvField->cellWidget(nrow, TBL_ATTR_FIELD_PRECISION));
        if(newField.type() != QVariant::Double) {
            newField.setPrecision(0);
        } else {

            newField.setPrecision(lineEditPrecision->text().toInt(&flagOk));
            if(!flagOk || newField.precision() < 0 || newField.precision() > FIELD_TYPE_PRECISION_MAX) {
                QMessageBox::critical(NULL, tr("���̓G���["), tr("�K�؂ȃt�B�[���h�̒������w�肵�Ă��������B[0 �` %2]").arg(FIELD_TYPE_PRECISION_MAX));
                lineEditPrecision->setFocus();
                return false;
            }
        }

        // �}�b�`���O�t�B�[���h�ɂȂ��Ă���ꍇ�́A���C���̃}�b�`���O�t�B�[���h�ɍ��킹��
//        if(fieldName == m_csvKeyName) {
        if(nrow == ui->cboCsvFileField->currentIndex()) {
            m_keyField = getField(m_newFieldMap, m_layerKeyName);
            if( m_keyField.type() != newField.type() ||
                m_keyField.length() != newField.length() ||
                m_keyField.precision() != newField.precision()) {

                newField.setType(m_keyField.type());
                newField.setLength(m_keyField.length());
                newField.setPrecision(m_keyField.precision());

                // �ύX���e����ʂɔ��f
                switch(newField.type()) {

                    case QVariant::String:
                        cboFieldType->setCurrentIndex(0);
                        break;

                    case QVariant::Int:
                    case QVariant::UInt:
                        cboFieldType->setCurrentIndex(1);
                        break;

                    case QVariant::Double:
                        cboFieldType->setCurrentIndex(2);
                        break;
                }

                lineEditLength->setText(tr("%1").arg(newField.length()));
                lineEditPrecision->setText(tr("%1").arg(newField.precision()));

                QMessageBox::information(this, tr(""), tr("�}�b�`���O�t�B�[���h�̌^������Ȃ����߁A���C���̃t�B�[���h�ɍ��킹�ĕύX���܂���"));
                this->repaint();
            }
        }

        // ���݂̃t�B�[���h���X�g�ɒǉ�
        m_newFieldMap.insert(m_newFieldMap.count(), newField);
    }

    return true;
}

// CSV��S���ǂݍ���
bool AttributeJoinDlg::loadCsvData(const QString& filename)
{
    QFile csvFile(filename);
    if (!csvFile.open(QIODevice::ReadOnly | QIODevice::Text)){
        QMessageBox::critical(NULL, tr("�t�@�C���I�[�v���G���["), tr("�e�L�X�g�t�@�C���̃I�[�v���Ɏ��s���܂���\n%1").arg(filename));
        return false;
    }

    int keyIndex = ui->cboCsvFileField->currentIndex();

    // csv�t�@�C����ǂݍ��݁A���Ԃɏ�������
    long    recCount = 0;

    //csv�t�@�C�����������Ƀ��[�h
    while(!csvFile.atEnd()) {

        recCount++;

        // CSV���P�s�ǂݍ���ō��ڂɕ����i������'\n'���폜����j
        QByteArray barray = csvFile.readLine();
        barray.remove(barray.count() - 1, 1);

        // �P�s�ڂ̓w�b�_�s�Ȃ̂œǂݔ�΂�
        if(recCount == 1) continue;

        QString lineBuf = barray;

        // ����ł͕������؂�Ȃ����_�u���N�H�[�e�[�V�����͕����Ƃ��ĔF��
        // �X�y�[�X������Ƃ��������Ȃ�
        QStringList data = CSV::parseLine(lineBuf, ui->radioTab->isChecked());

        // ���ڂ�����Ȃ��Ƃ��͕⊮����
        if(data.count() < ui->tblCsvField->rowCount()) {
            for(int i=data.count(); i<ui->tblCsvField->rowCount(); i++) {
                data.append(tr(""));
            }
        }

        m_csvData.insert(data.at(keyIndex), data);
        m_csvKey.append(data.at(keyIndex));
    }

    csvFile.close();

    return true;
}

QString AttributeJoinDlg::createFieldValueString(const QgsField& field, const QVariant& var)
{
    bool        flagOk;
    QString     retStr;

    switch(field.type()) {
        case QVariant::String:
        {
            retStr = var.toString();
            break;
        }
        case QVariant::Int:
        {
            int item = var.toInt(&flagOk);
            if(!flagOk) break;
            retStr.setNum(item);
            break;
        }
        case QVariant::UInt:
        {
            uint item = var.toUInt(&flagOk);
            if(!flagOk) break;
            retStr.setNum(item);
            break;
        }
        case QVariant::Double:
        {
            double item = var.toDouble(&flagOk);
            if(!flagOk) break;
            retStr.setNum(item, 'f', field.precision());
            break;
        }
    }

    return retStr;
}

// �����Ƃ̌�������
void AttributeJoinDlg::joinProc()
{
    QgsFeature orgFeature;

    // �J�[�\����BUSY�ɕύX
    QCursor orgCursor = this->cursor();
    setCursor(Qt::BusyCursor);

    // �����O����
    if(!preJoinProc()) {
        setCursor(orgCursor);
        return;
    }

    // �����܂łŐV�����쐬�����n���̑����t�B�[���h�}�b�v���쐬����Ă���

    // CSV�̓ǂݍ���
    if(!loadCsvData(m_csvFileName)) {
        setCursor(orgCursor);
        return;
    }

    // shape�����ɂ���Ȃ��U����
    if (QFile(m_shapeFileName).exists()) {
        QgsVectorFileWriter::deleteShapeFile(m_shapeFileName);
    }

    QGis::WkbType geometryType = m_vecLayer->wkbType();
    const QgsCoordinateReferenceSystem crs = m_vecLayer->crs();

    QgsVectorFileWriter *shapeFile = new QgsVectorFileWriter(m_shapeFileName, FILE_ENCODING, m_newFieldMap, geometryType, &crs);

    // �Ώۃ��C���̒n����S���ǂ�Ń}�b�`���O����
    // �n�����̃L�[���ڂ̃C���f�b�N�X���擾
    int layerKeyIndex = m_vecLayer->fieldNameIndex(m_layerKeyName);
    m_vecLayer->select(m_vecLayer->pendingAllAttributesList(), QgsRectangle(), true, false);

    QList<QVariant> csvRec;

    QString logInfo;
    m_errLog->open(m_unjoinedFileName);

    while(m_vecLayer->nextFeature(orgFeature)) {

        QgsFeature newFeature = orgFeature;

        // �n���̃L�[���ڂ̒l���擾
        QgsAttributeMap attrMap = newFeature.attributeMap();
        QgsAttributeMap::iterator iteAttr = attrMap.find(layerKeyIndex);
        assert(iteAttr != attrMap.end());
        QVariant featureKey = iteAttr.value();
//        QString keyVar = createFieldValueString(m_keyField, iteAttr.value());

        //CSV�}�b�v���猟��
        QMap< QString, QStringList >::iterator iteCsvRec;
//        iteCsvRec = m_csvData.find(keyVar);
        bool bFound = false;
        QString foundKey;
        for(iteCsvRec = m_csvData.begin(); iteCsvRec != m_csvData.end(); iteCsvRec++) {
            foundKey = iteCsvRec.key();
            QVariant csvKey = createFieldValue(m_keyField, foundKey);
            switch(csvKey.type()) {
                case QVariant::String:
                    if(csvKey.toString() == featureKey.toString()) {
                        bFound = true;
                    }
                    break;
                case QVariant::Int:
                    if(csvKey.toInt() == featureKey.toInt()) {
                        bFound = true;
                    }
                    break;
                case QVariant::UInt:
                    if(csvKey.toUInt() == featureKey.toUInt()) {
                        bFound = true;
                    }
                    break;
                case QVariant::Double:
                    if(csvKey.toDouble() == featureKey.toDouble()) {
                        bFound = true;
                    }
                    break;
            }
            if(bFound) break;
        }

//        if(iteCsvRec == m_csvData.end()) {
        if(!bFound) {
            // ���̂܂܏o��
        } else {
            //�^�ϊ�
            QStringList data = iteCsvRec.value();
            csvRec.clear();
            for(int i=0; i<data.count(); i++) {
                QgsFieldMap::iterator ite = m_newFieldMap.find(m_orgFieldMap.count() + i);
                QVariant var = createFieldValue(*ite, data.at(i));
                if(var.type() == QVariant::Invalid) {
                    //�G���[
                    QString errcsv = data.join("\",\"");
                    logInfo = tr("\"�t�B�[���h�̌^�ϊ��Ɏ��s ����(%1)\",\"%2\"\n").arg(i+1).arg(errcsv);
                    m_errLog->outLog(logInfo);
                    break;
                }
                csvRec.append(var);
            }

            // ��Q�ƃ��X�g�������
            for(int i=0; i<m_csvKey.count(); i++) {
                if(m_csvKey.at(i) == foundKey) {
                    m_csvKey.removeAt(i);
                    break;
                }
            }

            // CSV�̓��e��ǉ�
            for(int i=0; i<csvRec.count(); i++) {
                newFeature.addAttribute(newFeature.attributeMap().count(), csvRec.at(i));
            }
        }
        shapeFile->addFeature(newFeature);
    }

    delete shapeFile;

    //��Q�ƃ��X�g���o��
    for(int i=0; i<m_csvKey.count(); i++) {
        //CSV�}�b�v���猟��
        QMap< QString, QStringList >::iterator iteCsvRec;
        iteCsvRec = m_csvData.find(m_csvKey.at(i));
        if(!(iteCsvRec == m_csvData.end())) {
            QStringList data = iteCsvRec.value();
            QString errcsv = data.join("\",\"");
            logInfo = tr("\"�s��v���R�[�h\",\"%1\"\n").arg(errcsv);
            m_errLog->outLog(logInfo);
        }
    }

    m_errLog->close();

    if(ui->chkAddToProject->isChecked()) {
        int pos;
        for(pos=m_shapeFileName.length(); pos>=0; pos--) {
            if(m_shapeFileName.at(pos) == '\\' || m_shapeFileName.at(pos) == '/') {
                pos++;
                break;
            }
        }

        QString addLayerName = m_shapeFileName.mid(pos);
        mIface->addVectorLayer(m_shapeFileName, addLayerName, tr("ogr"));
    }

    QMessageBox::information(this, tr(""), tr("�������I�����܂���"));

    // �J�[�\�������ɖ߂�
    setCursor(orgCursor);
}

QgsVectorLayer* AttributeJoinDlg::getVectorLayerByName(QString layerName)
{
    QMap<QString, QgsMapLayer*> value = QgsMapLayerRegistry::instance()->mapLayers();
    QMap<QString, QgsMapLayer*>::const_iterator ite;

    for(ite = value.constBegin(); ite != value.constEnd(); ite++) {
        QgsMapLayer *layer = ite.value();
        if(layer->name() == layerName) {
            if(layer->type() != QgsMapLayer::VectorLayer) {
                // �x�N�^���C���łȂ�
                return NULL;
            }

            QgsVectorLayer* vecLayer = dynamic_cast<QgsVectorLayer*>(layer);
            return vecLayer;
        }
    }

    // ������Ȃ�
    return NULL;
}


bool AttributeJoinDlg::isExistField(QgsFieldMap fieldMap, const QString& fieldName)
{
    QgsFieldMap::iterator ite;


    for(ite = fieldMap.begin(); ite != fieldMap.end(); ite++) {
        if((*ite).name() == fieldName) {
            return true;
        }
    }

    return false;
}

QgsField AttributeJoinDlg::getField(QgsFieldMap fieldMap, const QString& fieldName)
{
    QgsFieldMap::iterator ite;


    for(ite = fieldMap.begin(); ite != fieldMap.end(); ite++) {
        if((*ite).name() == fieldName) {
            return *ite;
        }
    }

    return QgsField();
}

QVariant AttributeJoinDlg::createFieldValue(const QgsField& field, QString text)
{
    bool        flagOk;
    QVariant    var;
    QString     sItem;


    // �����^�͕ʏ���
    if(field.type() == QVariant::String) {
        return QString(text);
    }

    // ���l�^��CSV�̍��ڂ��ȗ����ꂽ�ꍇ�̑Ή�
    if(text.isEmpty()) {
        sItem = "0";
    } else {
        sItem = text;
    }

    // �ȉ��A���l�n�ŉ��������̕����񂪂���ꍇ
    var.clear();

    switch(field.type()) {

        case QVariant::Int:
        {
            // 1.6�Ƃ����؂�̂Ă�1�Ƃ��Đ���Ɏ󂯕t����悤�ɂ���
            double dItem = sItem.toDouble(&flagOk);
            if(flagOk && (double)INT_MIN <= dItem && dItem <= (double)INT_MAX) {
                var = (int)dItem;
//QMessageBox::information(this, tr("���l"), tr("%1").arg(item));
            }
            break;
        }
        case QVariant::UInt:
        {
            // 1.6�Ƃ����؂�̂Ă�1�Ƃ��Đ���Ɏ󂯕t����悤�ɂ���
            double dItem = sItem.toDouble(&flagOk);
            if(flagOk && 0.0 <= dItem && dItem <= (double)UINT_MAX) {
                var = (uint)dItem;
//QMessageBox::information(this, tr("���l"), tr("%1").arg(item));
            }
            break;
        }
        case QVariant::Double:
        {
            double dItem = sItem.toDouble(&flagOk);
            if(flagOk) {
                // �L�������ɍ��킹�ă`�F�b�N����
                double dLimit = powf(10, field.length() - field.precision());
                if(-dLimit < dItem && dItem < dLimit) {
                    double ds = powf(10.0, field.precision());
                    double imItem = floor(dItem);
                    double reItem = floor((dItem - imItem) * ds) / ds;
                    var = imItem + reItem;
//QMessageBox::information(this, tr("���l"), tr("%1").arg(item));
                }
            }
            break;
        }
    }

    return var;
}

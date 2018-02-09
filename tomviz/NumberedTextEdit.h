/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef tomvizNumberedTextEdit_h
#define tomvizNumberedTextEdit_h

#include <QTextEdit>

class NumberArea;

class NumberedTextEdit : public QTextEdit
{
  Q_OBJECT
public:
  NumberedTextEdit(QWidget* parent = 0);

  int numberAreaWidth();
  void numberAreaPaintEvent(QPaintEvent* event);
};

class NumberArea : public QWidget
{
public:
  NumberArea(NumberedTextEdit* parent) : QWidget(parent) {
    m_editor = parent;
  }

protected:
  void paintEvent(QPaintEvent* event) {
    m_editor->numberAreaPaintEvent(event);
  }

private:
  NumberedTextEdit* m_editor;
};

#endif
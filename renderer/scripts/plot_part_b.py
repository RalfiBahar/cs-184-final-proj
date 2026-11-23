"""Plot measured C++ output as SVG; requires reportlab."""
from pathlib import Path
import csv, math
from reportlab.graphics.shapes import Drawing, String, Rect
from reportlab.graphics.charts.lineplots import LinePlot
from reportlab.graphics import renderSVG
from reportlab.lib import colors
root=Path(__file__).resolve().parents[1]/'experiments'/'part_b'
with (root/'phase_histogram.csv').open() as f: rows=list(csv.DictReader(f))
with (root/'phase_summary.csv').open() as f: summary=list(csv.DictReader(f))
d=Drawing(1200,460)
d.add(Rect(0,0,1200,460,fillColor=colors.white,strokeColor=None))
d.add(String(35,422,'HG scattering: sampled angles match the expected distribution',fontName='Helvetica-Bold',fontSize=23))
for j,(g,col) in enumerate(zip([0,.5,.8],['#4263ad','#138a7b','#b56925'])):
 r=[x for x in rows if abs(float(x['g'])-g)<1e-14]
 samples=[((float(x['mu_left'])+float(x['mu_right']))/2,int(x['count'])/int(x['n'])/(float(x['mu_right'])-float(x['mu_left']))) for x in r]
 theory=[]
 for k in range(1001):
  mu=-1+2*k/1000
  theory.append((mu,(1-g*g)/(2*(1+g*g-2*g*mu)**1.5)))
 p=LinePlot();p.x=55+j*390;p.y=120;p.width=320;p.height=230;p.data=[theory,samples]
 p.xValueAxis.valueMin=-1;p.xValueAxis.valueMax=1;p.xValueAxis.valueSteps=[-1,-.5,0,.5,1]
 p.yValueAxis.valueMin=0;p.yValueAxis.valueMax=[.6,3.3,24][j]
 p.lines[0].strokeColor=colors.HexColor(col);p.lines[0].strokeWidth=2
 p.lines[1].strokeColor=colors.HexColor('#333333');p.lines[1].strokeWidth=1;p.lines[1].strokeDashArray=[3,3]
 d.add(p)
 d.add(String(p.x,375,f'g = {g:g}',fontName='Helvetica-Bold',fontSize=18))
 d.add(String(p.x+145,375,'Probability density',fontSize=12))
 s=next(x for x in summary if float(x['g'])==g)
 d.add(String(p.x,83,f"Mean: {float(s['mean_mu']):.4f}    Standard error: {float(s['standard_error']):.4f}",fontSize=12))
 d.add(String(p.x+100,96,'cos(scattering angle)',fontSize=11))
d.add(String(35,53,'Solid colored line: theory 2 pi p(mu).   Dashed gray line: measured histogram density.',fontSize=13))
d.add(String(35,30,'200,000 samples per g; seed 184; incoming direction proportional to (1, 2, 3).',fontSize=12))
d.add(String(35,11,'Module validation only. These are not integrated cloud renders. Vertical scales differ across panels.',fontSize=11,fillColor=colors.HexColor('#555555')))
renderSVG.drawToFile(d,str(root/'hg_validation.svg'))

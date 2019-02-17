### Created by Dr. Benjamin Richards (b.richards@qmul.ac.uk)

### Download base image from repo
FROM anniesoft/toolanalysis:base

### Run the following commands as super user (root):
USER root

Run cd ToolAnalysis; bash -c "source Setup.sh; cd ToolDAQ/MrdTrackLib/; git pull; make clean; cd ../../; make update; make;"

### Open terminal
CMD ["/bin/bash"]


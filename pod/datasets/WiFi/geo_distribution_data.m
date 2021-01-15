clear;
clc;

logs1 = csvread('logs_1.csv',1,4);
len = length(logs1);
workers = [];
for i=1:len
    workers = [workers; "Worker I"];
end

logs2 = csvread('logs_2.csv',1,4);
len = length(logs2);
for i=1:len
    workers = [workers; "Worker II"];
end

logs3 = csvread('logs_3.csv',1,4);
len = length(logs3);
for i=1:len
    workers = [workers; "Worker III"];
end

logs4 = csvread('logs_4.csv',1,4);
len = length(logs4);
for i=1:len
    workers = [workers; "Worker IV"];
end

logs5 = csvread('logs_5.csv',1,4);
len = length(logs5);
for i=1:len
    workers = [workers; "Worker V"];
end

logs6 = csvread('logs_6.csv',1,4);
len = length(logs6);
for i=1:len
    workers = [workers; "Worker VI"];
end

logs = [logs1;logs2;logs3;logs4;logs5;logs6];

colordata = categorical(workers);
gb = geobubble(logs(:,2),logs(:,1),logs(:,3),'Basemap','satellite','MapLayout','maximized');
gb.ColorData = colordata;
gb.ColorLegendTitle = 'Workers';
gb.SizeLegendTitle = 'Signal Strength';
gb.FontName = 'Times New Roman';
gb.FontSize = 14;
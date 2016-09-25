in = fopen('logs.csv');
fgets(in);%ignore csv header
A = textscan(in, '%d %s %s %s %s %s %d %d %d %d %d %d %d %d %d %d %d %s %f %s', 'Delimiter',',');
fclose(in);

set(0,'DefaultFigureVisible','off');
f = figure;
colors = cell(1,21);
ct = 0;
ct2 = 0;
last_rank = -1;
X=zeros(1,1);
Y=zeros(1,1);
labels = cell(1,21);
label_ct = 0;
for i = 1:size(A{1},1)
    if last_rank == -1
        last_rank = A{17}(i);
        last_label = A{18}{i};
    end
    if A{17}(i) ~= last_rank
        Y(ct2 + 1) = A{19}(i);
        X(ct2 + 1) = ct;
        if size(colors{last_rank},1) == 0
            h = plot(X,Y);
            colors{last_rank} = h.Color;
            label_ct = label_ct + 1;
            labels{label_ct} = last_label;
            last_label = A{18}{i};
        else
            plot(X,Y, 'Color', colors{last_rank});
        end
        X=zeros(1,1);
        Y=zeros(1,1);
        hold on;
        ct2 = 0;
        last_rank = A{17}(i);
    end
    ct = ct + 1;
    ct2 = ct2 + 1;
    Y(ct2) = A{19}(i);
    X(ct2) = ct;
end
if size(colors{last_rank},1) == 0
    h = plot(X,Y);
    label_ct = label_ct + 1;
    labels{label_ct} = last_label;
else
    h = plot(X,Y, 'Color', colors{last_rank});
end
xlabel('Game Number');
ylabel('Rating');
legend(labels{1:label_ct}, 'Location', 'Best');
saveas(f, 'rating_plot', 'png');
close(f);
    // ================================================================
    // Plotting — uses engine.figureManager() (member of Engine)
    // ================================================================
    //
    // Replace the old plotting stubs section in MLabStdLibrary.cpp
    // (from "// Plotting stubs" through "// 3D/specialized plots")
    // with this entire block.
    //
    // FigureManager is defined in MLabFigureManager.hpp and owned
    // by Engine. It resets automatically when Engine is recreated.
    // ================================================================

    auto &fm = engine.figureManager(); // reference for lambdas below

    // Helper: serialize a double vector/matrix to JSON array
    auto vecToJson = [](const MValue &v) -> std::string {
        std::ostringstream os;
        os << "[";
        if (v.isComplex()) {
            for (size_t i = 0; i < v.numel(); ++i) {
                if (i) os << ",";
                os << std::abs(v.complexData()[i]);
            }
        } else {
            for (size_t i = 0; i < v.numel(); ++i) {
                if (i) os << ",";
                double val = v.doubleData()[i];
                if (std::isnan(val)) os << "null";
                else if (std::isinf(val)) os << (val > 0 ? "1e308" : "-1e308");
                else os << val;
            }
        }
        os << "]";
        return os.str();
    };

    // Helper: generate x indices [1,2,...,n]
    auto makeIndexJson = [](size_t n) -> std::string {
        std::ostringstream xs;
        xs << "[";
        for (size_t i = 0; i < n; ++i) { if (i) xs << ","; xs << (i + 1); }
        xs << "]";
        return xs.str();
    };

    // Helper: extract string from MValue arg
    auto argStr = [](const MValue& v) -> std::string {
        return v.toString();
    };

    // --- figure() / figure(n) ---
    engine.registerFunction("figure",
        [&engine](Span<const MValue> args) -> std::vector<MValue> {
            auto *alloc = &engine.allocator();
            auto &fm = engine.figureManager();
            int id;
            if (args.empty()) {
                id = fm.newFigure();
            } else {
                id = static_cast<int>(args[0].toScalar());
                fm.setFigure(id);
            }
            out[0] = MValue::scalar(static_cast<double>(id), alloc); return;
        });

    // --- close() / close(n) / close('all') ---
    engine.registerFunction("close",
        [&engine](Span<const MValue> args) -> std::vector<MValue> {
            auto &fm = engine.figureManager();
            if (args.empty()) {
                fm.closeCurrent();
            } else if (args[0].isChar() && args[0].toString() == "all") {
                fm.closeAll();
            } else {
                fm.closeFigure(static_cast<int>(args[0].toScalar()));
            }
            out[0] = MValue::empty(); return;
        });

    // --- clf ---
    engine.registerFunction("clf",
        [&engine](const std::vector<MValue> &) -> std::vector<MValue> {
            auto &fig = engine.figureManager().current();
            fig.datasets.clear();
            fig.title.clear(); fig.xlabel.clear(); fig.ylabel.clear();
            fig.xlimJson.clear(); fig.ylimJson.clear();
            fig.grid = false; fig.polar = false; fig.legendLabels.clear();
            fig.holdOn = false;
            fig.modified = true;
            engine.figureManager().emitModified();
            out[0] = MValue::empty(); return;
        });

    // --- hold on / hold off / hold ---
    engine.registerFunction("hold",
        [&engine](Span<const MValue> args) -> std::vector<MValue> {
            auto &fig = engine.figureManager().current();
            if (args.empty()) {
                fig.holdOn = !fig.holdOn;
            } else {
                fig.holdOn = (args[0].toString() == "on");
            }
            out[0] = MValue::empty(); return;
        });

    // --- plot(x, y) / plot(y) / plot(x, y, style) ---
    engine.registerFunction("plot",
        [vecToJson, makeIndexJson, &engine](Span<const MValue> args) -> std::vector<MValue> {
            if (args.empty()) out[0] = MValue::empty(); return;
            auto &fm = engine.figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "line";
            if (args.size() >= 2 && !args[1].isChar()) {
                ds.xJson = vecToJson(args[0]);
                ds.yJson = vecToJson(args[1]);
                if (args.size() >= 3 && args[2].isChar())
                    ds.style = args[2].toString();
            } else {
                ds.xJson = makeIndexJson(args[0].numel());
                ds.yJson = vecToJson(args[0]);
                if (args.size() >= 2 && args[1].isChar())
                    ds.style = args[1].toString();
            }
            fm.current().datasets.push_back(std::move(ds));
            fm.emitModified();
            out[0] = MValue::empty(); return;
        });

    // --- bar(y) / bar(x, y) ---
    engine.registerFunction("bar",
        [vecToJson, makeIndexJson, &engine](Span<const MValue> args) -> std::vector<MValue> {
            if (args.empty()) out[0] = MValue::empty(); return;
            auto &fm = engine.figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "bar";
            if (args.size() >= 2 && !args[1].isChar()) {
                ds.xJson = vecToJson(args[0]);
                ds.yJson = vecToJson(args[1]);
            } else {
                ds.xJson = makeIndexJson(args[0].numel());
                ds.yJson = vecToJson(args[0]);
            }
            fm.current().datasets.push_back(std::move(ds));
            fm.emitModified();
            out[0] = MValue::empty(); return;
        });

    // --- scatter(x, y) ---
    engine.registerFunction("scatter",
        [vecToJson, &engine](Span<const MValue> args) -> std::vector<MValue> {
            if (args.size() < 2) out[0] = MValue::empty(); return;
            auto &fm = engine.figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "scatter";
            ds.xJson = vecToJson(args[0]);
            ds.yJson = vecToJson(args[1]);
            fm.current().datasets.push_back(std::move(ds));
            fm.emitModified();
            out[0] = MValue::empty(); return;
        });

    // --- hist(data, bins) ---
    engine.registerFunction("hist",
        [vecToJson, &engine](Span<const MValue> args) -> std::vector<MValue> {
            auto *alloc = &engine.allocator();
            if (args.empty()) out[0] = MValue::empty(); return;
            auto &data = args[0];
            int bins = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 10;
            double mn = data.doubleData()[0], mx = data.doubleData()[0];
            for (size_t i = 1; i < data.numel(); ++i) {
                mn = std::min(mn, data.doubleData()[i]);
                mx = std::max(mx, data.doubleData()[i]);
            }
            double bw = (mx - mn) / bins;
            if (bw == 0) bw = 1;
            auto centers = MValue::matrix(1, bins, MType::DOUBLE, alloc);
            auto counts = MValue::matrix(1, bins, MType::DOUBLE, alloc);
            for (int b = 0; b < bins; ++b)
                centers.doubleDataMut()[b] = mn + bw * (b + 0.5);
            for (size_t i = 0; i < data.numel(); ++i) {
                int b = static_cast<int>((data.doubleData()[i] - mn) / bw);
                if (b >= bins) b = bins - 1;
                if (b < 0) b = 0;
                counts.doubleDataMut()[b] += 1;
            }
            auto &fm = engine.figureManager();
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "bar";
            ds.xJson = vecToJson(centers);
            ds.yJson = vecToJson(counts);
            fm.current().datasets.push_back(std::move(ds));
            fm.emitModified();
            out[0] = MValue::empty(); return;
        });

    // --- polarplot(theta, rho) / polarplot(theta, rho, style) ---
    engine.registerFunction("polarplot",
        [vecToJson, &engine](Span<const MValue> args) -> std::vector<MValue> {
            if (args.size() < 2) out[0] = MValue::empty(); return;
            auto &fm = engine.figureManager();
            fm.prepareForPlot();
            fm.current().polar = true;
            DatasetInfo ds;
            ds.type = "line";
            ds.xJson = vecToJson(args[0]); // theta (radians)
            ds.yJson = vecToJson(args[1]); // rho
            if (args.size() >= 3 && args[2].isChar())
                ds.style = args[2].toString();
            fm.current().datasets.push_back(std::move(ds));
            fm.emitModified();
            out[0] = MValue::empty(); return;
        });

    // --- title('text') ---
    engine.registerFunction("title",
        [argStr, &engine](Span<const MValue> args) -> std::vector<MValue> {
            if (!args.empty()) {
                auto &fm = engine.figureManager();
                fm.current().title = argStr(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            out[0] = MValue::empty(); return;
        });

    // --- xlabel('text') ---
    engine.registerFunction("xlabel",
        [argStr, &engine](Span<const MValue> args) -> std::vector<MValue> {
            if (!args.empty()) {
                auto &fm = engine.figureManager();
                fm.current().xlabel = argStr(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            out[0] = MValue::empty(); return;
        });

    // --- ylabel('text') ---
    engine.registerFunction("ylabel",
        [argStr, &engine](Span<const MValue> args) -> std::vector<MValue> {
            if (!args.empty()) {
                auto &fm = engine.figureManager();
                fm.current().ylabel = argStr(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            out[0] = MValue::empty(); return;
        });

    // --- xlim([min, max]) ---
    engine.registerFunction("xlim",
        [vecToJson, &engine](Span<const MValue> args) -> std::vector<MValue> {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = engine.figureManager();
                fm.current().xlimJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            out[0] = MValue::empty(); return;
        });

    // --- ylim([min, max]) ---
    engine.registerFunction("ylim",
        [vecToJson, &engine](Span<const MValue> args) -> std::vector<MValue> {
            if (!args.empty() && args[0].numel() >= 2) {
                auto &fm = engine.figureManager();
                fm.current().ylimJson = vecToJson(args[0]);
                fm.current().modified = true;
                fm.emitModified();
            }
            out[0] = MValue::empty(); return;
        });

    // --- grid on / grid off / grid ---
    engine.registerFunction("grid",
        [&engine](Span<const MValue> args) -> std::vector<MValue> {
            auto &fm = engine.figureManager();
            auto &fig = fm.current();
            if (args.empty()) {
                fig.grid = !fig.grid;
            } else {
                fig.grid = (args[0].toString() == "on");
            }
            fig.modified = true;
            fm.emitModified();
            out[0] = MValue::empty(); return;
        });

    // --- legend('a', 'b', ...) ---
    engine.registerFunction("legend",
        [argStr, &engine](Span<const MValue> args) -> std::vector<MValue> {
            auto &fm = engine.figureManager();
            auto &fig = fm.current();
            fig.legendLabels.clear();
            for (auto &a : args)
                fig.legendLabels.push_back(argStr(a));
            fig.modified = true;
            fm.emitModified();
            out[0] = MValue::empty(); return;
        });

    // ================================================================
    // Remaining GUI no-ops
    // ================================================================
    auto noop = [](const std::vector<MValue> &) -> std::vector<MValue> { out[0] = MValue::empty(); return; };
    auto noop_ret1 = [&engine](const std::vector<MValue> &) -> std::vector<MValue> {
        out[0] = MValue::scalar(1.0, &engine.allocator()); return;
    };

    engine.registerFunction("subplot", noop);
    engine.registerFunction("axes", noop_ret1);
    engine.registerFunction("gca", noop_ret1);
    engine.registerFunction("gcf", noop_ret1);
    engine.registerFunction("cla", noop);
    engine.registerFunction("zlabel", noop);
    engine.registerFunction("colorbar", noop);
    engine.registerFunction("colormap", noop);
    engine.registerFunction("caxis", noop);
    engine.registerFunction("clim", noop);
    engine.registerFunction("zlim", noop);
    engine.registerFunction("rlim", noop);
    engine.registerFunction("axis", noop);
    engine.registerFunction("view", noop);
    engine.registerFunction("set", noop);
    engine.registerFunction("get", noop_ret1);

    engine.registerFunction("scatter3", noop);
    engine.registerFunction("surf", noop);
    engine.registerFunction("mesh", noop);
    engine.registerFunction("contour", noop);
    engine.registerFunction("contourf", noop);
    engine.registerFunction("imagesc", noop);
    engine.registerFunction("pcolor", noop);
    engine.registerFunction("xline", noop);
    engine.registerFunction("yline", noop);
    engine.registerFunction("camlight", noop);
    engine.registerFunction("lighting", noop);
